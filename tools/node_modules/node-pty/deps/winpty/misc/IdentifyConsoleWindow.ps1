#
# Usage: powershell <path>\IdentifyConsoleWindow.ps1
#
# This script determines whether the process has a console attached, whether
# that console has a non-NULL window (e.g. HWND), and whether the window is on
# the current window station.
#

$signature = @'
[DllImport("kernel32.dll", SetLastError=true)]
public static extern IntPtr GetConsoleWindow();

[DllImport("kernel32.dll", CharSet=CharSet.Auto, SetLastError=true)]
public static extern bool SetConsoleTitle(String title);

[DllImport("user32.dll", CharSet=CharSet.Auto, SetLastError=true)]
public static extern int GetWindowText(IntPtr hWnd,
                                       System.Text.StringBuilder lpString,
                                       int nMaxCount);
'@

$WinAPI = Add-Type -MemberDefinition $signature `
    -Name WinAPI -Namespace IdentifyConsoleWindow -PassThru

if (!$WinAPI::SetConsoleTitle("ConsoleWindowScript")) {
    echo "error: could not change console title -- is a console attached?"
    exit 1
} else {
    echo "note: successfully set console title to ""ConsoleWindowScript""."
}

$hwnd = $WinAPI::GetConsoleWindow()
if ($hwnd -eq 0) {
    echo "note: GetConsoleWindow returned NULL."
} else {
    echo "note: GetConsoleWindow returned 0x$($hwnd.ToString("X"))."
    $sb = New-Object System.Text.StringBuilder -ArgumentList 4096
    if ($WinAPI::GetWindowText($hwnd, $sb, $sb.Capacity)) {
        $title = $sb.ToString()
        echo "note: GetWindowText returned ""${title}""."
        if ($title -eq "ConsoleWindowScript") {
            echo "success!"
        } else {
            echo "error: expected to see ""ConsoleWindowScript""."
            echo "  (Perhaps the console window is on a different window station?)"
        }
    } else {
        echo "error: GetWindowText could not read the window title."
        echo "  (Perhaps the console window is on a different window station?)"
    }
}
