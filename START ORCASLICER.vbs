Option Explicit

Dim fileSystem, shell, scriptDirectory, powerShellPath, wingetPath
Dim installCommand, command, argument, exitCode

Set fileSystem = CreateObject("Scripting.FileSystemObject")
Set shell = CreateObject("WScript.Shell")

scriptDirectory = fileSystem.GetParentFolderName(WScript.ScriptFullName)
powerShellPath = FindPowerShell()

If Len(powerShellPath) = 0 Then
    wingetPath = shell.ExpandEnvironmentStrings("%LOCALAPPDATA%") & _
        "\Microsoft\WindowsApps\winget.exe"
    If fileSystem.FileExists(wingetPath) Then
        installCommand = Quote(wingetPath) & _
            " install --id Microsoft.PowerShell --exact --source winget" & _
            " --silent --accept-package-agreements --accept-source-agreements" & _
            " --disable-interactivity"
        exitCode = shell.Run(installCommand, 0, True)
        If exitCode = 0 Then powerShellPath = FindPowerShell()
    End If
End If

If Len(powerShellPath) = 0 Then
    ' Windows cannot self-bootstrap without either PowerShell or WinGet.
    shell.Run "https://learn.microsoft.com/powershell/scripting/install/installing-powershell-on-windows", 1, False
    WScript.Quit 2
End If

command = Quote(powerShellPath) & _
    " -NoLogo -NoProfile -ExecutionPolicy Bypass -WindowStyle Hidden -File " & _
    Quote(fileSystem.BuildPath(scriptDirectory, _
        "helper_scripts\orcaslicer_launcher\launcher.ps1"))

For Each argument In WScript.Arguments
    command = command & " " & Quote(argument)
Next

' Window style 0 keeps the PowerShell host hidden. OrcaSlicer's WinForms UI and
' any OrcaSlicer application launched from it remain normal visible windows.
exitCode = shell.Run(command, 0, True)
WScript.Quit exitCode

Function Quote(value)
    Quote = Chr(34) & Replace(value, Chr(34), Chr(34) & Chr(34)) & Chr(34)
End Function

Function FindPowerShell()
    Dim candidate
    candidate = shell.ExpandEnvironmentStrings("%SystemRoot%") & _
        "\System32\WindowsPowerShell\v1.0\powershell.exe"
    If fileSystem.FileExists(candidate) Then
        FindPowerShell = candidate
        Exit Function
    End If

    candidate = shell.ExpandEnvironmentStrings("%ProgramFiles%") & _
        "\PowerShell\7\pwsh.exe"
    If fileSystem.FileExists(candidate) Then
        FindPowerShell = candidate
        Exit Function
    End If

    candidate = shell.ExpandEnvironmentStrings("%LOCALAPPDATA%") & _
        "\Microsoft\WindowsApps\pwsh.exe"
    If fileSystem.FileExists(candidate) Then
        FindPowerShell = candidate
        Exit Function
    End If

    FindPowerShell = ""
End Function
