"""
pack_task.py — GUI: pick a roadmap task, get a pastable context pack.

Closes the manual gap between route_tasks.py (parses roadmap.md, stamps
**Route:**) and context_pack.py (needs an explicit file list typed on the
command line). This wraps both behind one window: refresh routing -> check
one or more tasks in a sortable/filterable table -> preview the assembled
pack -> copy / save / send.

Usage:
    python helper_scripts/pack_task.py

No new dependencies: tkinter is stdlib; network calls reuse query_model.py's
existing urllib-based client.
"""

import contextlib
import json
import os
import queue
import re
import sys
import threading
import tkinter as tk
import traceback
from dataclasses import dataclass, field
from pathlib import Path
from tkinter import filedialog, messagebox, scrolledtext, ttk

sys.path.insert(0, str(Path(__file__).resolve().parent))
import context_pack  # noqa: E402
import query_model  # noqa: E402
import route_tasks  # noqa: E402

SCRIPT_ROOT = Path(__file__).resolve().parent.parent
DEFAULT_ROADMAP = SCRIPT_ROOT / "docs" / "roadmap.md"
DEFAULT_SHIPPED = SCRIPT_ROOT / "docs" / "shipped.md"
# Forward-only high-water mark for **Effort:**, persisted here because docs/shipped.md
# doesn't retain Effort once a task ships — this file is the only durable record of the
# highest Effort ever seen, so the combined-score column's 1-5 effort scale (see
# _combined_score) stays stable across sessions instead of rescaling every refresh.
EFFORT_CEILING_PATH = Path(__file__).resolve().parent / "effort_ceiling.txt"

HEADER_RE = re.compile(r"^- \[([ x])\]\s*\*\*(.+?)\*\*")
# Splits a header (per roadmap.md's format convention) into its packed attributes:
# "Score <1-5> <medal> · Class <1-3> — <title>" -> score, title (class is already
# pulled separately via route_tasks.CLASS_RE; the medal glyph is matched generically
# and not trusted — the picker recomputes it from score via route_tasks._medal()).
HEADER_ATTR_RE = re.compile(
    r"^Score\s+(?P<score>[1-5])\s+\S+\s*·\s*Class\s+(?P<cls>[123])\s*—\s*(?P<title>.+)$"
)
# Any ## or ### heading line, used to tag each task with the section it falls under
# (e.g. "Upstream architecture study") so the picker's filter box can match on it —
# a task's own title often doesn't contain its section's theme word.
SECTION_RE = re.compile(r"^#{2,4}\s+(.+?)\s*$")
FILE_LIST_RE = re.compile(
    r"^\s+\*\*(Implement|Write|Context|Read)[^*]*\*\*:?\s*(.+)", re.IGNORECASE
)
ROUTE_VALUE_RE = re.compile(r"^\s*\*\*Route:\*\*\s*(.+?)\s*$")
EFFORT_VALUE_RE = re.compile(r"^\s*\*\*Effort:\*\*\s*(.+?)\s*$")
CHARS_VALUE_RE = re.compile(r"^\s*\*\*Chars:\*\*\s*(.+?)\s*$")
FENCE_START_RE = re.compile(r"^\s*```text\s*$")
FENCE_END_RE = re.compile(r"^\s*```\s*$")

CHECK_EMPTY = "☐"  # ☐
CHECK_FULL = "☑"  # ☑


def split_header(header: str) -> tuple[str, str]:
    """Break a task header into (score, title), for table columns."""
    m = HEADER_ATTR_RE.match(header)
    if not m:
        return "", header
    return m.group("score"), m.group("title")


def _score_int(t: "Task") -> int:
    score, _ = split_header(t.header)
    return int(score) if score.isdigit() else 0


def _effort_int(t: "Task") -> int | None:
    return int(t.effort) if t.effort.isdigit() else None


def _read_effort_ceiling() -> int:
    try:
        return max(1, int(EFFORT_CEILING_PATH.read_text(encoding="utf-8").strip()))
    except (FileNotFoundError, ValueError):
        return 1


def _update_effort_ceiling(tasks: list["Task"]) -> int:
    """Raise the persisted ceiling if any currently-open task's Effort exceeds it; never
    lowers it. Returns the effective ceiling to normalize against."""
    stored = _read_effort_ceiling()
    seen_max = max((_effort_int(t) or 0 for t in tasks), default=0)
    ceiling = max(stored, seen_max)
    if ceiling != stored:
        EFFORT_CEILING_PATH.write_text(str(ceiling), encoding="utf-8")
    return ceiling


def _combined_score(t: "Task", effort_ceiling: int) -> float:
    """Value-density, with effort normalized onto the same 1-5 scale as score (the
    persisted effort_ceiling maps to 5) before dividing — otherwise effort's much larger
    raw range (1 to 1000+) swamps score's 1-5 range and the ratio is effectively just
    1/effort. Squared, not linear: a task twice as complex costs more than twice as much
    once you're past small/mechanical size (route_tasks.py's own tier bumps aren't linear
    either). 0 when effort isn't known yet (unrouted)."""
    effort = _effort_int(t)
    score = _score_int(t)
    if effort is None or score == 0:
        return 0.0
    effort_scaled = 1 + 4 * (min(effort, effort_ceiling) / effort_ceiling) ** 2
    return score / effort_scaled


def _model_breakdown(t: "Task") -> bool:
    """True when the task got bumped all the way to claude purely for size (too many
    files or too many chars — route_tasks.py's own routing rules), not because it's
    Class 3. This is the dividing line where free/local models stop being viable at all,
    as opposed to gemini->kimi bumps which just move within free-tier capacity."""
    return t.task_class != "3" and t.route == "claude"


def _default_sort_key(t: "Task") -> tuple[int, int]:
    """Highest score first; lowest effort breaks ties within the same score.
    Unrouted (unknown) effort sorts last within its score tier."""
    effort = _effort_int(t)
    return (-_score_int(t), effort if effort is not None else 10**9)


# Offline seed/fallback for the OpenRouter model combobox: the "Fetch free models" button
# (query_model._openrouter_free_models) pulls the live ':free' list at runtime, so this
# list only matters when offline or before a fetch. Verified live 2026-07-05 (the previous
# llama-3.1-8b / qwen-2.5-72b / gemma-2-9b IDs were all retired).
OPENROUTER_MODELS = [
    "openrouter/qwen/qwen3-coder:free",
    "openrouter/deepseek/deepseek-r1:free",
    "openrouter/meta-llama/llama-3.3-70b-instruct:free",
]


# Opt-in OpenRouter key persistence. Stored in the user's home dir — deliberately OUTSIDE
# any repo, so a plaintext secret can never be accidentally committed (the earlier design
# kept the key session-only for exactly this reason; persistence is now opt-in via the
# "Remember" checkbox, off by default, and a home-dir file is the safe way to allow it).
OPENROUTER_KEY_FILE = Path.home() / ".pmt_openrouter_key"


def _load_saved_key() -> str:
    try:
        return OPENROUTER_KEY_FILE.read_text(encoding="utf-8").strip()
    except (FileNotFoundError, OSError):
        return ""


def _save_key(key: str) -> None:
    OPENROUTER_KEY_FILE.write_text(key, encoding="utf-8")
    # best-effort; a no-op on Windows
    with contextlib.suppress(OSError):
        os.chmod(OPENROUTER_KEY_FILE, 0o600)


def _forget_saved_key() -> None:
    with contextlib.suppress(FileNotFoundError, OSError):
        OPENROUTER_KEY_FILE.unlink()


# Mirrors context_pack.py's own --max-chars default (docs/ai-harness.md §3.1):
# packs under this size stay a single paste, larger ones split between files.
CHUNK_MAX_CHARS = 100_000


def _chunk_budget(routes):
    """Tightest known per-message char budget among the given model routes, capped at
    CHUNK_MAX_CHARS — so a pack auto-chunks at whichever routed model's real paste
    ceiling is smallest, instead of always waiting for the generic 100k mark. A Kimi-
    routed pack sailing through at ~46k chars unchunked (well past Kimi's own measured
    ceiling, see docs/ai-harness.md §1) is exactly the failure this closes."""
    known = [
        context_pack.MODEL_BUDGETS[r]["chars"]
        for r in routes
        if r in context_pack.MODEL_BUDGETS and context_pack.MODEL_BUDGETS[r]["chars"] is not None
    ]
    return min([CHUNK_MAX_CHARS, *known]) if known else CHUNK_MAX_CHARS


@dataclass
class Task:
    header: str
    task_class: str
    section: str = ""  # nearest preceding ##/### heading this task falls under
    route: str = "?"
    effort: str = "?"
    files: list[Path] = field(default_factory=list)  # resolved, existing paths, in file-list order
    missing: list[str] = field(
        default_factory=list
    )  # raw backtick strings that didn't resolve to a file
    prompt: str = ""  # extracted ```text block body, if the item has one
    start: int = -1  # index into the parsed lines[] of this task's header
    end: int = -1  # exclusive end index (route_tasks.scan_block_end)


def parse_tasks(lines: list[str], root: Path) -> list[Task]:
    """Walk freshly-routed roadmap.md lines into open Task objects."""
    tasks = []
    i = 0
    section = ""
    while i < len(lines):
        sm = SECTION_RE.match(lines[i])
        if sm:
            section = sm.group(1)
            i += 1
            continue

        m = HEADER_RE.match(lines[i])
        if not m or m.group(1) == "x":
            i += 1
            continue

        class_m = route_tasks.CLASS_RE.search(lines[i])
        task = Task(
            header=m.group(2),
            task_class=class_m.group(1) if class_m else "?",
            section=section,
            start=i,
        )

        # Shared fence-aware block scan (route_tasks.scan_block_end) — so this parser
        # and route_tasks._process()/find_task_block() can't disagree about where one
        # task ends and the next begins.
        j = route_tasks.scan_block_end(lines, i + 1)
        task.end = j
        block = lines[i + 1 : j]

        seen = set()
        in_fence = False
        fence_lines = []
        body_lines = []
        for bl in block:
            rm = ROUTE_VALUE_RE.match(bl)
            if rm:
                task.route = rm.group(1)
                continue
            em = EFFORT_VALUE_RE.match(bl)
            if em:
                task.effort = em.group(1)
                continue
            if CHARS_VALUE_RE.match(bl):
                continue
            fm = FILE_LIST_RE.match(bl)
            if fm:
                files, missing = route_tasks.resolve_file_list(fm.group(2), root)
                for p in files:
                    key = p.as_posix()
                    if key in seen:
                        continue
                    seen.add(key)
                    task.files.append(p)
                for raw in missing:
                    if raw in seen:
                        continue
                    seen.add(raw)
                    task.missing.append(raw)
                continue
            if FENCE_START_RE.match(bl):
                in_fence = True
                continue
            if in_fence and FENCE_END_RE.match(bl):
                in_fence = False
                continue
            if in_fence:
                fence_lines.append(bl)
            else:
                body_lines.append(bl)

        # A fenced ```text block (a ready-made, model-facing prompt) takes priority when
        # present; most items have no fence at all, so fall back to the plain description
        # paragraph under the header — otherwise those tasks packed with an empty prompt box.
        task.prompt = "".join(fence_lines).strip()
        if not task.prompt:
            # header_tail is the word-wrapped start of the same sentence the body
            # continues (the roadmap's line-wrap convention splits mid-sentence, not
            # at a paragraph boundary) — join with a space, not a blank line, or it
            # reads as two sentence fragments instead of one.
            header_tail = lines[i][m.end() :].strip(" \t\n-—")
            desc = "".join(body_lines).strip()
            task.prompt = f"{header_tail} {desc}".strip() if header_tail else desc
        tasks.append(task)
        i = j
    return tasks


class App(tk.Tk):
    def __init__(self):
        super().__init__()
        import argparse

        p = argparse.ArgumentParser(add_help=False)
        p.add_argument("--roadmap")
        p.add_argument("--shipped")
        cli, _ = p.parse_known_args()
        self.ROADMAP_PATH = (
            Path(cli.roadmap).expanduser().resolve() if cli.roadmap else DEFAULT_ROADMAP
        )
        self.SHIPPED_PATH = (
            Path(cli.shipped).expanduser().resolve() if cli.shipped else DEFAULT_SHIPPED
        )
        self.title("Quest Board")
        self.geometry("960x720")
        self.root_dir = context_pack.repo_root()
        self.result_queue = queue.Queue()
        self.tasks = []
        self.filtered_tasks = []
        self.roadmap_lines = []  # backing lines for task.start/task.end — set by _load_tasks
        self.effort_ceiling = _read_effort_ceiling()
        self.checked = set()  # iids (str index into filtered_tasks) checked in the picker table
        self.current_tasks = []  # the task(s) behind the pack currently shown on the pack screen
        self.chunks = []  # paste-sized chunks of the current pack, banner-wrapped
        self.chunk_idx = 0
        # When checked (default), the pack screen shows one concatenated block, even
        # past a model's char budget — e.g. to deliberately spend one of chatgpt's
        # file-upload slots (docs/ai-harness.md §1) instead of pasting in chunks. When
        # unchecked, the pack screen instead shows only the paginated chunk view (large
        # Prev/Next) so a monster paste never lands in the box by surprise. Lives on the
        # pack screen (not the picker) since it only controls that screen's display mode;
        # chunks are always computed in _on_pack regardless of this value.
        self.ignore_char_limit = tk.BooleanVar(value=True)

        # send/settings screen state. Most of it is session-only; the OpenRouter key is
        # the exception — it may be persisted, but only when the user opts in via the
        # "Remember" checkbox (see _on_remember_toggled). The key is prefilled from the
        # OPENROUTER_API_KEY env var first, then a previously-saved file, so the env var
        # still wins and default behavior is unchanged when nothing was saved.
        self.send_target = tk.StringVar(value="qwen")
        self.send_endpoint = tk.StringVar(value=self._default_endpoint("qwen"))
        self.openrouter_models = list(
            OPENROUTER_MODELS
        )  # combobox values; replaced by a live fetch
        self.send_model_or = tk.StringVar(value=self.openrouter_models[0])
        _saved_key = _load_saved_key()
        self.send_api_key = tk.StringVar(
            value=os.environ.get("OPENROUTER_API_KEY", "") or _saved_key
        )
        self.remember_key = tk.BooleanVar(
            value=bool(_saved_key)
        )  # ticked if a key was previously saved
        self.send_max_tokens = tk.StringVar(value="4096")
        self.qwen_detected_model = None  # set by "Detect Now", used only for the preview

        self.picker_frame = ttk.Frame(self, padding=12)
        self.pack_frame = ttk.Frame(self, padding=12)
        self.send_frame = ttk.Frame(self, padding=12)
        self._build_picker_frame()
        self._build_pack_frame()
        self._build_send_frame()
        self._load_tasks()
        self.picker_frame.pack(fill="both", expand=True)

    def refresh_routes(self) -> list[str]:
        """Re-stamp **Route:**/**Chars:** in roadmap.md (same effect as route_tasks.py's
        default run). The write goes through route_tasks.write_roadmap — the same truncation
        sanity check and atomic temp-file-then-rename as the CLI — so the GUI never has a
        laxer write path than the CLI. Raises RuntimeError if the output looks truncated."""
        text = self.ROADMAP_PATH.read_text(encoding="utf-8")
        lines = text.splitlines(keepends=True)
        updated = route_tasks._process(lines, context_pack.repo_root())
        route_tasks.write_roadmap(self.ROADMAP_PATH, lines, updated)
        return updated

    # ── picker screen ──

    def _build_picker_frame(self):
        f = self.picker_frame

        nav = ttk.Frame(f)
        nav.pack(fill="x", pady=(0, 6))
        ttk.Button(nav, text="Pack →", command=self._on_pack).pack(side="right")

        ttk.Label(
            f,
            text="Pick one or more roadmap tasks (check the box to include in the pack):",
            font=("", 11, "bold"),
        ).pack(anchor="w")

        search_row = ttk.Frame(f)
        search_row.pack(fill="x", pady=(4, 4))
        self.filter_var = tk.StringVar()
        self.filter_var.trace_add("write", lambda *_: self._apply_filter())
        ttk.Entry(search_row, textvariable=self.filter_var).pack(side="left", fill="x", expand=True)
        ttk.Button(search_row, text="×", width=3, command=lambda: self.filter_var.set("")).pack(
            side="left", padx=(4, 0)
        )

        tree_frame = ttk.Frame(f)
        tree_frame.pack(fill="both", expand=True, pady=(0, 6))

        columns = (
            "sel",
            "score",
            "effort",
            "combined",
            "class",
            "route",
            "files",
            "section",
            "title",
        )
        # (heading label, width, whether this column absorbs extra widget width)
        col_spec = {
            "sel": ("", 26, False),
            "score": ("Score", 60, False),
            "effort": ("Effort", 55, False),
            "combined": ("Value/Effort", 80, False),
            "class": ("Class", 50, False),
            "route": ("Route", 80, False),
            "files": ("Files", 60, False),
            "section": ("Section", 170, False),
            "title": ("Title", 400, True),
        }
        self.tree = ttk.Treeview(tree_frame, columns=columns, show="headings", selectmode="browse")
        for col in columns:
            label, width, stretch = col_spec[col]
            self.tree.column(col, width=width, anchor="w", stretch=stretch)
            if col == "sel":
                self.tree.heading(col, text=label)  # not sortable — it's the checkbox column
            else:
                self.tree.heading(col, text=label, command=lambda c=col: self._sort_by(c, False))
        vsb = ttk.Scrollbar(tree_frame, orient="vertical", command=self.tree.yview)
        self.tree.configure(yscrollcommand=vsb.set)
        self.tree.pack(side="left", fill="both", expand=True)
        vsb.pack(side="left", fill="y")
        self.tree.bind("<Button-1>", self._on_tree_click)

        btn_row = ttk.Frame(f)
        btn_row.pack(fill="x", pady=(0, 6))
        ttk.Button(btn_row, text="Refresh", command=self._load_tasks).pack(side="left")
        ttk.Button(btn_row, text="Check All", command=lambda: self._set_all_checked(True)).pack(
            side="left", padx=6
        )
        ttk.Button(btn_row, text="Clear", command=lambda: self._set_all_checked(False)).pack(
            side="left"
        )
        ttk.Button(
            btn_row,
            text="Mark Complete →",
            command=self._on_mark_complete,
        ).pack(side="left", padx=(20, 0))

    def _load_tasks(self):
        self.tasks = []
        self.filtered_tasks = []
        try:
            lines = self.refresh_routes()
        except FileNotFoundError:
            messagebox.showerror("roadmap.md not found", f"Could not find {self.ROADMAP_PATH}")
            self.tree.delete(*self.tree.get_children())
            return
        except RuntimeError as e:
            # write_roadmap refused a truncated-looking rewrite — the file on disk is
            # untouched; surface the refusal instead of showing silently stale routes.
            messagebox.showerror("Roadmap refresh failed", str(e))
            self.tree.delete(*self.tree.get_children())
            return
        self.roadmap_lines = lines  # what task.start/task.end index into — see _on_mark_complete
        self.tasks = parse_tasks(lines, self.root_dir)
        self.effort_ceiling = _update_effort_ceiling(self.tasks)
        self._apply_filter()

    def _apply_filter(self):
        q = self.filter_var.get().strip().lower()
        if q:
            # Matches header/title, section heading, class, and route — not just the
            # header. A research task's own title rarely contains its section's theme
            # word; that word only lives in the section heading above it.
            self.filtered_tasks = [
                t
                for t in self.tasks
                if q in t.header.lower()
                or q in t.section.lower()
                or q in t.task_class.lower()
                or q in t.route.lower()
            ]
        else:
            self.filtered_tasks = list(self.tasks)

        # Default order: highest score first, lowest effort breaks ties among equally-
        # scored tasks. Re-applied on every load/filter change; an explicit header click
        # below overrides it until the next load/filter.
        self.filtered_tasks.sort(key=_default_sort_key)

        self.checked.clear()
        self.tree.delete(*self.tree.get_children())
        for idx, t in enumerate(self.filtered_tasks):
            score, title = split_header(t.header)
            score_col = f"{score} {route_tasks._medal(score)}" if score else ""
            files_col = str(len(t.files)) + (f" (+{len(t.missing)})" if t.missing else "")
            combined = _combined_score(t, self.effort_ceiling)
            combined_col = f"{combined:.2f}" if combined else ""
            # ⚠ marks tasks bumped to claude purely for size — past the point where
            # free/local models are viable at all, not just a routing preference.
            route_col = t.route
            if _model_breakdown(t):
                route_col += " ⚠"
            elif t.route.lower().startswith("me"):
                route_col = "👤 me"
            self.tree.insert(
                "",
                "end",
                iid=str(idx),
                values=(
                    CHECK_EMPTY,
                    score_col,
                    t.effort,
                    combined_col,
                    t.task_class,
                    route_col,
                    files_col,
                    t.section,
                    title,
                ),
            )
        children = self.tree.get_children()
        if children:
            self.tree.focus(children[0])
            self.tree.selection_set(children[0])

    NUMERIC_COLS = {"score", "effort", "combined", "files"}

    @staticmethod
    def _cell_sort_key(col, value):
        """Numeric columns sort on their leading number (score's medal glyph and files'
        "N (+M)" suffix are ignored), not lexicographically — "780" must sort after "120",
        not before it. Unknown/blank cells (e.g. unrouted effort) sort as lowest."""
        if col in App.NUMERIC_COLS:
            m = re.match(r"\d+(\.\d+)?", value.strip())
            return float(m.group()) if m else -1.0
        return value.lower()

    def _sort_by(self, col, reverse):
        """Click-to-sort a column header. Rows keep their iid (the filtered_tasks index),
        so this only reorders the display — checked state and _selected_tasks still resolve
        correctly afterward."""
        rows = [(self.tree.set(iid, col), iid) for iid in self.tree.get_children()]
        rows.sort(key=lambda r: self._cell_sort_key(col, r[0]), reverse=reverse)
        for pos, (_, iid) in enumerate(rows):
            self.tree.move(iid, "", pos)
        self.tree.heading(col, command=lambda: self._sort_by(col, not reverse))

    def _on_tree_click(self, event):
        if self.tree.identify_region(event.x, event.y) != "cell":
            return
        row = self.tree.identify_row(event.y)
        if not row:
            return
        if self.tree.identify_column(event.x) == "#1":  # the checkbox column
            self._toggle_checked(row)

    def _toggle_checked(self, iid):
        if iid in self.checked:
            self.checked.discard(iid)
            self.tree.set(iid, "sel", CHECK_EMPTY)
        else:
            self.checked.add(iid)
            self.tree.set(iid, "sel", CHECK_FULL)

    def _set_all_checked(self, state):
        for iid in self.tree.get_children():
            if state:
                self.checked.add(iid)
                self.tree.set(iid, "sel", CHECK_FULL)
            else:
                self.checked.discard(iid)
                self.tree.set(iid, "sel", CHECK_EMPTY)

    def _selected_tasks(self):
        """Checked rows if any are checked, else the single focused/clicked row."""
        if self.checked:
            idxs = sorted(int(i) for i in self.checked)
        else:
            focus = self.tree.focus()
            idxs = [int(focus)] if focus else []
        return [self.filtered_tasks[i] for i in idxs]

    def _on_mark_complete(self):
        """Human override for route_tasks.py's --complete: mark the checked/focused
        task(s) done and move them into shipped.md, gated behind an explicit warning
        since this removes content from the roadmap and can't be undone from here."""
        tasks = self._selected_tasks()
        if not tasks:
            messagebox.showinfo(
                "No task selected",
                "Check one or more tasks (or click a row) before marking complete.",
            )
            return

        titles = "\n".join(f"- {split_header(t.header)[1]}" for t in tasks)
        if not messagebox.askyesno(
            "Mark complete?",
            f"Mark {len(tasks)} task(s) as done and move them into {self.SHIPPED_PATH.name}?\n\n"
            f"{titles}\n\n"
            "This removes them from the active roadmap and cannot be undone from within "
            "Quest Board (you'd have to edit the files by hand to reverse it). Continue?",
        ):
            return

        shipped_lines = (
            self.SHIPPED_PATH.read_text(encoding="utf-8").splitlines(keepends=True)
            if self.SHIPPED_PATH.is_file()
            else []
        )
        roadmap_lines = list(self.roadmap_lines)
        # Remove back-to-front so each task's (start, end) indices — computed against
        # the original roadmap_lines — stay valid as earlier removals shift later lines.
        for t in sorted(tasks, key=lambda t: t.start, reverse=True):
            roadmap_lines, shipped_lines = route_tasks.move_to_shipped(
                roadmap_lines,
                shipped_lines,
                t.start,
                t.end,
            )

        # Write shipped.md first: a crash between the two writes leaves a recoverable
        # duplicate rather than deleting a task with nothing recorded in shipped.md.
        route_tasks._atomic_write(self.SHIPPED_PATH, "".join(shipped_lines))
        route_tasks._atomic_write(self.ROADMAP_PATH, "".join(roadmap_lines))
        self._load_tasks()

    # ── pack screen ──

    def _build_pack_frame(self):
        f = self.pack_frame

        topnav = ttk.Frame(f)
        topnav.pack(side="top", fill="x", pady=(0, 6))
        ttk.Button(topnav, text="← Back", command=self._on_back_to_picker).pack(side="left")
        ttk.Button(topnav, text="Configure & Send →", command=self._on_goto_send).pack(side="right")

        # Bottom-anchored action row — packed (side="bottom") before the expandable
        # middle content below, so it always claims its space at the bottom of the
        # frame instead of being pushed off-screen when the paginated chunk view
        # (taller than the single pack_text box) is showing.
        btns = ttk.Frame(f)
        btns.pack(side="bottom", fill="x", pady=(6, 0))
        ttk.Button(btns, text="Copy to Clipboard", command=self._on_copy).pack(side="left")
        ttk.Button(btns, text="Save to Text", command=self._on_save).pack(side="left", padx=6)

        self.status_label = ttk.Label(f, text="", foreground="#555", wraplength=900)
        self.status_label.pack(anchor="w")

        self.fit_label = ttk.Label(f, text="", foreground="#555", wraplength=900)
        self.fit_label.pack(anchor="w", pady=(0, 6))

        ttk.Checkbutton(
            f,
            text="Ignore char limit (show one concatenated block instead of paged chunks — "
            "burns file-upload budget instead of chunking)",
            variable=self.ignore_char_limit,
            command=self._refresh_pack_view,
        ).pack(anchor="w", pady=(0, 6))

        # pack_text (checked / default) and chunk_frame (unchecked) occupy the same
        # spot in the layout — only one is packed at a time, toggled in _refresh_pack_view().
        self.pack_text = scrolledtext.ScrolledText(f, wrap="word", height=18)

        style = ttk.Style(self)
        style.configure("Big.TButton", font=("", 16, "bold"), padding=14)

        # Paginated view (docs/ai-harness.md §3.1) — one page at a time via large
        # Prev/Next, so a pack that would otherwise be a monster paste never lands in
        # the box unannounced. Independent of prompt_text: Copy/Save below keep
        # working on the whole pack regardless of which view is showing.
        self.chunk_frame = ttk.Frame(f)
        ttk.Label(
            self.chunk_frame,
            text='Paged view — step through with Prev/Next below. Use "Copy This Chunk" / '
            '"Copy && Advance" to paste each page as its own chat message, in order, '
            "then the prompt as the closing message (docs/ai-harness.md §3.1):",
            wraplength=900,
        ).pack(anchor="w")
        chunknav = ttk.Frame(self.chunk_frame)
        chunknav.pack(fill="x", pady=(4, 4))
        self.btn_chunk_prev = ttk.Button(
            chunknav, text="<< Prev", style="Big.TButton", command=self._on_prev_chunk
        )
        self.btn_chunk_prev.pack(side="left")
        self.chunk_label = ttk.Label(chunknav, text="", font=("", 11, "bold"))
        self.chunk_label.pack(side="left", padx=12)
        self.btn_chunk_next = ttk.Button(
            chunknav, text="Next >>", style="Big.TButton", command=self._on_next_chunk
        )
        self.btn_chunk_next.pack(side="left")
        ttk.Button(chunknav, text="Copy This Chunk", command=self._on_copy_chunk).pack(
            side="left", padx=(20, 0)
        )
        ttk.Button(chunknav, text="Copy && Advance", command=self._on_copy_chunk_advance).pack(
            side="left", padx=6
        )
        self.chunk_preview = scrolledtext.ScrolledText(self.chunk_frame, wrap="word", height=18)
        self.chunk_preview.pack(fill="both", expand=True, pady=(8, 8))

        self.prompt_label = ttk.Label(f, text="Prompt / task description (editable):")
        self.prompt_label.pack(anchor="w")
        self.prompt_text = scrolledtext.ScrolledText(f, wrap="word", height=6)
        self.prompt_text.pack(fill="x", pady=(2, 8))

    def _on_pack(self):
        tasks = self._selected_tasks()
        if not tasks:
            messagebox.showinfo(
                "No task selected", "Check one or more tasks (or click a row) before packing."
            )
            return

        files = []
        seen = set()
        for t in tasks:
            for p in t.files:
                key = p.as_posix()
                if key not in seen:
                    seen.add(key)
                    files.append(p)

        body, total_lines, per_file, bodies = context_pack.build_pack(
            files, self.root_dir, fence=True
        )
        self.current_tasks = tasks

        self.pack_text.delete("1.0", "end")
        self.pack_text.insert("1.0", body)

        prompt_body = "\n\n".join(t.prompt for t in tasks if t.prompt)
        external_missing = []
        seen_external = set()
        for t in tasks:
            for m in t.missing:
                if route_tasks.external_ref_name(m) and m not in seen_external:
                    seen_external.add(m)
                    external_missing.append(m)
        if external_missing:
            lines = []
            for m in external_missing:
                hint = route_tasks.external_search_hint(m)
                if hint:
                    lines.append(f"- {m} — search: {hint}")
                else:
                    name = route_tasks.external_ref_name(m)
                    lines.append(
                        f"- {m} — unregistered reference `{name}`; search for it "
                        f"yourself, or add a hint to helper_scripts/external_refs.json"
                    )
            prompt_body += (
                "\n\nThe following referenced files are not bundled in the context pack above "
                "(no local clone configured on this machine). Search for them yourself before "
                "answering. If a lookup fails or isn't available, don't refuse the task — answer "
                "from what you already know and mark those specific claims [UNVERIFIED]:\n"
                + "\n".join(lines)
            )
        self.prompt_text.delete("1.0", "end")
        self.prompt_text.insert("1.0", prompt_body)

        # Always chunked, regardless of ignore_char_limit — that checkbox (on the pack
        # screen) only toggles which view is displayed; see _refresh_pack_view. Chunk size
        # is model-aware (_chunk_budget) so a tight-budget route (e.g. kimi) splits before
        # its own paste ceiling, not just the generic 100k mark.
        file_chunks = context_pack.chunk_bodies(bodies, _chunk_budget({t.route for t in tasks}))
        self.chunks = [
            context_pack.wrap_chunk(fc, i + 1, len(file_chunks)) for i, fc in enumerate(file_chunks)
        ]
        self._refresh_pack_view()

        self.status_label.config(text=self._status_text(tasks, per_file, total_lines, len(body)))
        self.fit_label.config(text=self._fit_text(per_file, len(body)))
        self.picker_frame.pack_forget()
        self.pack_frame.pack(fill="both", expand=True)

    def _status_text(self, tasks, per_file, total_lines, n_chars):
        classes = sorted({t.task_class for t in tasks})
        routes = sorted({t.route for t in tasks})
        title = tasks[0].header if len(tasks) == 1 else f"{len(tasks)} tasks"
        parts = [
            f"{title} · Class {'/'.join(classes)} · routed to {'/'.join(routes)} · "
            f"{len(per_file)} files, {total_lines} lines, {n_chars} chars"
        ]
        if any(r.lower().startswith("me") for r in routes):
            parts.append("[human-routed — pack for maintainer context only]")
        if not per_file:
            parts.append(
                "[warning: no resolvable file paths in selected task(s) — packing prompt only, no file context]"
            )
        missing = [m for t in tasks for m in t.missing]
        # `<name>` external-reference entries are a distinct case from genuine new/placeholder
        # paths: they're not missing because they don't exist yet, but because no local clone
        # is configured for that name (route_tasks.EXTERNAL_REF_PATHS_FILE) — say so, or "not
        # on disk yet" reads as if these files were supposed to be created by this task.
        external_missing = [m for m in missing if route_tasks.external_ref_name(m)]
        other_missing = [m for m in missing if not route_tasks.external_ref_name(m)]
        if external_missing:
            parts.append(
                f"[{len(external_missing)} external reference file(s) not bundled — search "
                f"hints included in the prompt below for the AI to find them itself; or add a "
                f"local clone path to helper_scripts/external_ref_paths.txt to bundle them instead]"
            )
        if other_missing:
            shown = ", ".join(other_missing[:3])
            more = "..." if len(other_missing) > 3 else ""
            parts.append(
                f"[{len(other_missing)} listed path(s) not on disk yet (new/placeholder), excluded: {shown}{more}]"
            )
        for route in routes:
            budget = context_pack.MODEL_BUDGETS.get(route)
            if budget and budget["files"] == 0:
                parts.append(
                    f"[{route} takes no file uploads — paste the pack text directly into chat]"
                )
            if route in context_pack.MODEL_BUDGETS:
                label, detail = context_pack.model_fitness(per_file, n_chars)[route]
                if label != "fits":
                    parts.append(f"[WARNING: routed model {route} — {detail}]")
        if "3" in classes:
            parts.append(
                "[CLASS 3 — safety-critical: model output is research only, never applied without author review]"
            )
        return "  ".join(parts)

    def _fit_text(self, per_file, n_chars):
        """One label per model in MODEL_BUDGETS, so a model that's outright unusable for this
        exact pack (e.g. chatgpt when a single file alone exceeds its char budget) is visible
        at a glance, not just the one model route_tasks.py happened to route this task to."""
        tags = {"fits": "OK", "trim": "TRIM", "not viable": "NOT VIABLE"}
        parts = []
        for model, (label, detail) in context_pack.model_fitness(per_file, n_chars).items():
            tag = tags[label]
            parts.append(f"{model}: {tag}" + (f" ({detail})" if detail else ""))
        return "Model fit — " + " · ".join(parts)

    def _on_back_to_picker(self):
        self.pack_frame.pack_forget()
        self.picker_frame.pack(fill="both", expand=True)

    def _refresh_pack_view(self):
        """Toggle between the full-text box (checked) and the paginated chunk stepper
        (unchecked) — only one occupies the pack screen's main content area at a time."""
        if self.ignore_char_limit.get():
            self.chunk_frame.pack_forget()
            self.pack_text.pack(fill="both", expand=True, pady=(6, 6), before=self.prompt_label)
        else:
            self.pack_text.pack_forget()
            self.chunk_frame.pack(fill="both", expand=True, pady=(6, 6), before=self.prompt_label)
            self._show_chunk(0)

    def _show_chunk(self, idx):
        self.chunk_idx = idx
        total = len(self.chunks)
        self.chunk_preview.delete("1.0", "end")
        if total == 0:
            self.chunk_preview.insert("1.0", "(no file content — prompt only)")
            self.chunk_label.config(text="No chunks")
            self.btn_chunk_prev.state(["disabled"])
            self.btn_chunk_next.state(["disabled"])
            return
        self.chunk_preview.insert("1.0", self.chunks[idx])
        self.chunk_label.config(text=f"Chunk {idx + 1} of {total}")
        self.btn_chunk_prev.state(["disabled"] if idx == 0 else ["!disabled"])
        self.btn_chunk_next.state(["disabled"] if idx == total - 1 else ["!disabled"])

    def _on_prev_chunk(self):
        if self.chunk_idx > 0:
            self._show_chunk(self.chunk_idx - 1)

    def _on_next_chunk(self):
        if self.chunk_idx < len(self.chunks) - 1:
            self._show_chunk(self.chunk_idx + 1)

    def _on_copy_chunk(self):
        if not self.chunks:
            return
        self.clipboard_clear()
        self.clipboard_append(self.chunks[self.chunk_idx])
        self._append_status(f"[copied chunk {self.chunk_idx + 1} of {len(self.chunks)}]")

    def _on_copy_chunk_advance(self):
        self._on_copy_chunk()
        if self.chunk_idx < len(self.chunks) - 1:
            self._show_chunk(self.chunk_idx + 1)

    def _combined_text(self):
        """Prompt plus whichever pack view is currently active — the full concatenated
        block when ignore_char_limit is checked, or just the on-screen chunk when it's
        not, so Copy/Save actually track what pagination is showing instead of always
        grabbing the whole pack behind the paginated view's back."""
        prompt = self.prompt_text.get("1.0", "end").strip()
        if self.ignore_char_limit.get():
            pack = self.pack_text.get("1.0", "end").strip()
        else:
            pack = self.chunks[self.chunk_idx].strip() if self.chunks else ""
        return (prompt + "\n\n---\n\n" + pack) if prompt else pack

    def _on_copy(self):
        self.clipboard_clear()
        self.clipboard_append(self._combined_text())
        self._append_status("[copied to clipboard]")

    def _on_save(self):
        path = filedialog.asksaveasfilename(
            defaultextension=".txt",
            initialfile="scratch.txt",
            initialdir=str(self.root_dir),
        )
        if not path:
            return
        Path(path).write_text(self._combined_text(), encoding="utf-8")
        self._append_status(f"[saved to {path}]")

    def _append_status(self, extra):
        self.status_label.config(text=self.status_label.cget("text") + "  " + extra)

    def _poll_result(self):
        try:
            status, payload = self.result_queue.get_nowait()
        except queue.Empty:
            self.after(200, self._poll_result)
            return
        self.btn_send.state(["!disabled"])
        if status == "error":
            self.send_status_label.config(text="[send failed]")
            messagebox.showerror("Send failed", payload)
            return
        self.send_status_label.config(text="[response received]")
        self._show_response(payload)

    def _show_response(self, text):
        win = tk.Toplevel(self)
        win.title("Model response")
        win.geometry("820x640")
        box = scrolledtext.ScrolledText(win, wrap="word")
        box.pack(fill="both", expand=True)
        box.insert("1.0", text)

        def copy():
            self.clipboard_clear()
            self.clipboard_append(text)

        def save():
            path = filedialog.asksaveasfilename(defaultextension=".md", initialfile="response.md")
            if path:
                Path(path).write_text(text, encoding="utf-8")

        btns = ttk.Frame(win)
        btns.pack(fill="x")
        ttk.Button(btns, text="Copy", command=copy).pack(side="left", padx=6, pady=6)
        ttk.Button(btns, text="Save", command=save).pack(side="left")
        ttk.Button(btns, text="Close", command=win.destroy).pack(side="right", padx=6)

    # ── send / settings screen ──

    @staticmethod
    def _default_endpoint(target):
        if target == "qwen":
            return os.environ.get("QWEN_LM_STUDIO_URL", "http://localhost:1234/v1").rstrip("/")
        return "https://openrouter.ai/api/v1"

    @staticmethod
    def _mask_key(key):
        if not key:
            return "(none set)"
        if len(key) <= 8:
            return "*" * len(key)
        return f"{key[:4]}...{key[-4:]}"

    def _on_goto_send(self):
        self.pack_frame.pack_forget()
        self.send_frame.pack(fill="both", expand=True)
        self._refresh_send_preview()

    def _on_back_from_send(self):
        self.send_frame.pack_forget()
        self.pack_frame.pack(fill="both", expand=True)

    def _build_send_frame(self):
        f = self.send_frame

        topnav = ttk.Frame(f)
        topnav.pack(side="top", fill="x", pady=(0, 6))
        ttk.Button(topnav, text="← Back", command=self._on_back_from_send).pack(side="left")

        bottom = ttk.Frame(f)
        bottom.pack(side="bottom", fill="x", pady=(6, 0))
        self.btn_send = ttk.Button(
            bottom, text="Send", style="Big.TButton", command=self._on_send_from_settings
        )
        self.btn_send.pack(side="left")
        self.send_status_label = ttk.Label(bottom, text="", foreground="#555")
        self.send_status_label.pack(side="left", padx=12)

        ttk.Label(
            f,
            text="Where this pack gets sent, and what it'll look like on the wire:",
            font=("", 11, "bold"),
        ).pack(anchor="w", pady=(0, 6))

        target_row = ttk.Frame(f)
        target_row.pack(anchor="w", pady=(0, 6))
        ttk.Label(target_row, text="Send target:").pack(side="left")
        ttk.Radiobutton(
            target_row,
            text="Qwen (local LM Studio)",
            value="qwen",
            variable=self.send_target,
            command=self._on_target_changed,
        ).pack(side="left", padx=(6, 12))
        ttk.Radiobutton(
            target_row,
            text="OpenRouter (cloud)",
            value="openrouter",
            variable=self.send_target,
            command=self._on_target_changed,
        ).pack(side="left")

        endpoint_row = ttk.Frame(f)
        endpoint_row.pack(fill="x", pady=(0, 6))
        ttk.Label(endpoint_row, text="Endpoint URL:").pack(side="left")
        ttk.Entry(endpoint_row, textvariable=self.send_endpoint, width=50).pack(side="left", padx=6)
        ttk.Button(endpoint_row, text="Reset to default", command=self._reset_endpoint).pack(
            side="left"
        )

        self.model_row = ttk.Frame(f)
        self.model_row.pack(fill="x", pady=(0, 6))

        key_row = ttk.Frame(f)
        key_row.pack(fill="x", pady=(0, 6))
        ttk.Label(key_row, text="OpenRouter API key:").pack(side="left")
        self.api_key_entry = ttk.Entry(key_row, textvariable=self.send_api_key, show="*", width=40)
        self.api_key_entry.pack(side="left", padx=6)
        self.remember_check = ttk.Checkbutton(
            key_row,
            text=f"Remember on this machine (plaintext {OPENROUTER_KEY_FILE.name} in home dir)",
            variable=self.remember_key,
            command=self._on_remember_toggled,
        )
        self.remember_check.pack(side="left", padx=(6, 0))

        tokens_row = ttk.Frame(f)
        tokens_row.pack(fill="x", pady=(0, 6))
        ttk.Label(tokens_row, text="Max tokens:").pack(side="left")
        ttk.Entry(tokens_row, textvariable=self.send_max_tokens, width=8).pack(side="left", padx=6)

        ttk.Button(f, text="Refresh Preview", command=self._refresh_send_preview).pack(
            anchor="w", pady=(0, 4)
        )
        ttk.Label(f, text="Request preview (API key masked):").pack(anchor="w")
        self.send_preview = scrolledtext.ScrolledText(f, wrap="word", height=18, state="disabled")
        self.send_preview.pack(fill="both", expand=True, pady=(2, 0))

        self._on_target_changed()

    def _build_model_row(self):
        """Rebuilds model_row's contents for the current target — a free-text combobox
        for openrouter, or a read-only detected value + Detect button for qwen, whose
        model comes from LM Studio's /v1/models rather than being chosen here."""
        for child in self.model_row.winfo_children():
            child.destroy()
        ttk.Label(self.model_row, text="Model:").pack(side="left")
        if self.send_target.get() == "openrouter":
            ttk.Combobox(
                self.model_row,
                textvariable=self.send_model_or,
                values=self.openrouter_models,
                width=36,
            ).pack(side="left", padx=6)
            ttk.Button(
                self.model_row,
                text="Fetch free models",
                command=self._fetch_openrouter_models,
            ).pack(side="left", padx=(6, 0))
        else:
            label = self.qwen_detected_model or "(auto-detected at send time)"
            ttk.Label(self.model_row, text=label, foreground="#555").pack(side="left", padx=6)
            ttk.Button(self.model_row, text="Detect Now", command=self._detect_qwen_model).pack(
                side="left"
            )

    def _reset_endpoint(self):
        self.send_endpoint.set(self._default_endpoint(self.send_target.get()))
        self._refresh_send_preview()

    def _on_target_changed(self):
        target = self.send_target.get()
        # Only auto-swap the endpoint if it still holds a known default (i.e. the user
        # hasn't customized it) — don't clobber a deliberately edited URL.
        known_defaults = {self._default_endpoint("qwen"), self._default_endpoint("openrouter")}
        if self.send_endpoint.get() in known_defaults:
            self.send_endpoint.set(self._default_endpoint(target))
        self.api_key_entry.state(["!disabled"] if target == "openrouter" else ["disabled"])
        self.remember_check.state(["!disabled"] if target == "openrouter" else ["disabled"])
        self._build_model_row()
        self._refresh_send_preview()

    def _detect_qwen_model(self):
        base_url = self.send_endpoint.get().strip().rstrip("/")
        try:
            self.qwen_detected_model = query_model._lm_studio_model(base_url)
        except RuntimeError as e:
            messagebox.showerror("Detect failed", str(e))
            return
        self._build_model_row()
        self._refresh_send_preview()

    def _fetch_openrouter_models(self):
        """Pull the live list of ':free' OpenRouter models and repopulate the combobox,
        so the hardcoded OPENROUTER_MODELS seed never has to be hand-verified. Mirrors
        _detect_qwen_model — no API key required (the models endpoint is public)."""
        base_url = self.send_endpoint.get().strip().rstrip("/")
        api_key = self.send_api_key.get().strip()
        try:
            models = query_model._openrouter_free_models(base_url, api_key or None)
        except RuntimeError as e:
            messagebox.showerror("Fetch failed", str(e))
            return
        if not models:
            messagebox.showinfo(
                "No free models", "OpenRouter returned no ':free' models right now."
            )
            return
        self.openrouter_models = models
        if self.send_model_or.get() not in models:
            self.send_model_or.set(models[0])
        self._build_model_row()
        self._refresh_send_preview()
        self.send_status_label.config(text=f"[fetched {len(models)} free model(s)]")

    def _current_send_settings(self):
        """(target, base_url, model_id_or_placeholder, api_key, max_tokens) from the
        settings screen's current field values."""
        target = self.send_target.get()
        base_url = self.send_endpoint.get().strip().rstrip("/")
        if target == "qwen":
            model_id = self.qwen_detected_model or "(auto-detected at send time)"
        else:
            model_id = self.send_model_or.get().strip()
        api_key = self.send_api_key.get().strip()
        try:
            max_tokens = int(self.send_max_tokens.get().strip())
        except ValueError:
            max_tokens = 4096
        return target, base_url, model_id, api_key, max_tokens

    def _build_request_payload(self, model_id, max_tokens):
        # Reuses _combined_text() so the request sent/previewed here is exactly what the
        # pack screen (page 2) shows and lets you Copy/Save — including which chunk is on
        # screen when the paginated view is active, not always the full unpaginated pack.
        user_msg = self._combined_text()
        return user_msg, {
            "model": model_id,
            "messages": [
                {"role": "system", "content": query_model._SYSTEM},
                {"role": "user", "content": user_msg},
            ],
            "temperature": 0,
            "max_tokens": max_tokens,
        }

    def _refresh_send_preview(self):
        target, base_url, model_id, api_key, max_tokens = self._current_send_settings()
        _, payload = self._build_request_payload(model_id, max_tokens)
        headers = {"Content-Type": "application/json"}
        if target == "openrouter":
            headers["Authorization"] = f"Bearer {self._mask_key(api_key)}"
        preview = {"url": f"{base_url}/chat/completions", "headers": headers, "body": payload}
        text = json.dumps(preview, indent=2)
        self.send_preview.configure(state="normal")
        self.send_preview.delete("1.0", "end")
        self.send_preview.insert("1.0", text)
        self.send_preview.configure(state="disabled")

    def _on_remember_toggled(self):
        """Opt-in key persistence. Checked -> save the current key to the home-dir file;
        unchecked -> delete any saved copy. The file lives outside the repo so a plaintext
        secret can't be committed; it's still plaintext on disk, hence off by default."""
        if self.remember_key.get():
            key = self.send_api_key.get().strip()
            if key:
                _save_key(key)
        else:
            _forget_saved_key()

    def _on_send_from_settings(self):
        if any(t.task_class == "3" for t in self.current_tasks) and not messagebox.askyesno(
            "Class 3 task",
            "This is a Class 3 (top review tier) task. Per this project's CLAUDE.md, no "
            "free-tier model output may be applied to Class 3 code without author review "
            "and any checklist that tier requires — this send is for read-only research "
            "only. Continue?",
        ):
            return
        if any(t.route.lower().startswith("me") for t in self.current_tasks):
            messagebox.showinfo("Human-routed task", "Sending this to a model probably won't work.")
            return

        target, base_url, model_id, api_key, max_tokens = self._current_send_settings()
        prompt = self.prompt_text.get("1.0", "end").strip()
        if not prompt:
            messagebox.showwarning(
                "No prompt", "Write a task description in the prompt box before sending."
            )
            return
        if target == "openrouter" and not api_key:
            messagebox.showwarning("No API key", "Enter an OpenRouter API key before sending.")
            return
        # Keep the saved copy current with whatever's in the field now, if the user opted
        # to remember it (they may have typed or edited the key after ticking the box).
        if target == "openrouter" and self.remember_key.get() and api_key:
            _save_key(api_key)

        self.btn_send.state(["disabled"])
        self.send_status_label.config(text=f"sending to {model_id} ...")

        def worker():
            try:
                if target == "qwen":
                    resolved_model = query_model._lm_studio_model(base_url)
                    headers = {}
                else:
                    resolved_model = model_id
                    headers = {"Authorization": f"Bearer {api_key}"}
                user_msg, _ = self._build_request_payload(resolved_model, max_tokens)
                resp = query_model._chat(
                    base_url, resolved_model, headers, query_model._SYSTEM, user_msg, max_tokens
                )
                self.result_queue.put(("ok", resp))
            except Exception as e:  # noqa: BLE001 - message goes to the user; traceback goes to the console
                traceback.print_exc()
                self.result_queue.put(("error", str(e)))

        threading.Thread(target=worker, daemon=True).start()
        self.after(200, self._poll_result)


def main():
    App().mainloop()


if __name__ == "__main__":
    main()
