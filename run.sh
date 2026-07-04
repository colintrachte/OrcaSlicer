#!/usr/bin/env bash
# OrcaSlicer launch script for Linux and macOS

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"

BUILD_CONFIG="Release"
BUILD_DIR="build"

while getopts ":beh" opt; do
    case ${opt} in
        b ) BUILD_CONFIG="Debug"; BUILD_DIR="build-dbg" ;;
        e ) BUILD_CONFIG="RelWithDebInfo"; BUILD_DIR="build-dbginfo" ;;
        h )
            echo "Usage: ./run.sh [-b][-e]"
            echo "   -b: launch the Debug build"
            echo "   -e: launch the RelWithDebInfo build"
            exit 0 ;;
        \? ) echo "Unknown option: -$OPTARG" >&2; exit 1 ;;
    esac
done

if [[ "$(uname)" == "Darwin" ]]; then
    APP="$SCRIPT_DIR/$BUILD_DIR/OrcaSlicer/OrcaSlicer.app"
    if [[ ! -d "$APP" ]]; then
        echo "ERROR: OrcaSlicer.app not found at $APP"
        echo "Build the project first with: ./build_release_macos.sh -s"
        exit 1
    fi
    echo "Launching $APP"
    open "$APP"
else
    BIN="$SCRIPT_DIR/$BUILD_DIR/OrcaSlicer/OrcaSlicer"
    if [[ ! -f "$BIN" ]]; then
        echo "ERROR: OrcaSlicer binary not found at $BIN"
        echo "Build the project first with: ./build_linux.sh -ds"
        exit 1
    fi
    echo "Launching $BIN"
    "$BIN" "$@"
fi
