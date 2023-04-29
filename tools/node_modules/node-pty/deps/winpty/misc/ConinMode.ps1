#
# PowerShell script for controlling the console QuickEdit and InsertMode flags.
#
# Turn QuickEdit off to interact with mouse-driven console programs.
#
# Usage:
#
# powershell .\ConinMode.ps1 [Options]
#
# Options:
#   -QuickEdit [on/off]
#   -InsertMode [on/off]
#   -Mode [integer]
#

param (
    [ValidateSet("on", "off")][string] $QuickEdit,
    [ValidateSet("on", "off")][string] $InsertMode,
    [int] $Mode
)

$signature = @'
[DllImport("kernel32.dll", SetLastError = true)]
public static extern IntPtr GetStdHandle(int nStdHandle);

[DllImport("kernel32.dll", SetLastError = true)]
public static extern uint GetConsoleMode(
    IntPtr hConsoleHandle,
    out uint lpMode);

[DllImport("kernel32.dll", SetLastError = true)]
public static extern uint SetConsoleMode(
    IntPtr hConsoleHandle,
    uint dwMode);

public const int STD_INPUT_HANDLE = -10;
public const int ENABLE_INSERT_MODE = 0x0020;
public const int ENABLE_QUICK_EDIT_MODE = 0x0040;
public const int ENABLE_EXTENDED_FLAGS = 0x0080;
'@

$WinAPI = Add-Type -MemberDefinition $signature `
    -Name WinAPI -Namespace ConinModeScript `
    -PassThru

function GetConIn {
    $ret = $WinAPI::GetStdHandle($WinAPI::STD_INPUT_HANDLE)
    if ($ret -eq -1) {
        throw "error: cannot get stdin"
    }
    return $ret
}

function GetConsoleMode {
    $conin = GetConIn
    $mode = 0
    $ret = $WinAPI::GetConsoleMode($conin, [ref]$mode)
    if ($ret -eq 0) {
        throw "GetConsoleMode failed (is stdin a console?)"
    }
    return $mode
}

function SetConsoleMode($mode) {
    $conin = GetConIn
    $ret = $WinAPI::SetConsoleMode($conin, $mode)
    if ($ret -eq 0) {
        throw "SetConsoleMode failed (is stdin a console?)"
    }
}

$oldMode = GetConsoleMode
$newMode = $oldMode
$doingSomething = $false

if ($PSBoundParameters.ContainsKey("Mode")) {
    $newMode = $Mode
    $doingSomething = $true
}

if ($QuickEdit + $InsertMode -ne "") {
    if (!($newMode -band $WinAPI::ENABLE_EXTENDED_FLAGS)) {
        # We can't enable an extended flag without overwriting the existing
        # QuickEdit/InsertMode flags.  AFAICT, there is no way to query their
        # existing values, so at least we can choose sensible defaults.
        $newMode = $newMode -bor $WinAPI::ENABLE_EXTENDED_FLAGS
        $newMode = $newMode -bor $WinAPI::ENABLE_QUICK_EDIT_MODE
        $newMode = $newMode -bor $WinAPI::ENABLE_INSERT_MODE
        $doingSomething = $true
    }
}

if ($QuickEdit -eq "on") {
    $newMode = $newMode -bor $WinAPI::ENABLE_QUICK_EDIT_MODE
    $doingSomething = $true
} elseif ($QuickEdit -eq "off") {
    $newMode = $newMode -band (-bnot $WinAPI::ENABLE_QUICK_EDIT_MODE)
    $doingSomething = $true
}

if ($InsertMode -eq "on") {
    $newMode = $newMode -bor $WinAPI::ENABLE_INSERT_MODE
    $doingSomething = $true
} elseif ($InsertMode -eq "off") {
    $newMode = $newMode -band (-bnot $WinAPI::ENABLE_INSERT_MODE)
    $doingSomething = $true
}

if ($doingSomething) {
    echo "old mode: $oldMode"
    SetConsoleMode $newMode
    $newMode = GetConsoleMode
    echo "new mode: $newMode"
} else {
    echo "mode: $oldMode"
}
