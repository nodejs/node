# Next Version

Input handling changes:

 * Improve Ctrl-C handling with programs that use unprocessed input. (e.g.
   Ctrl-C now cancels input with PowerShell on Windows 10.)
   [#116](https://github.com/rprichard/winpty/issues/116)
 * Fix a theoretical issue with input event ordering.
   [#117](https://github.com/rprichard/winpty/issues/117)
 * Ctrl/Shift+{Arrow,Home,End} keys now work with IntelliJ.
   [#118](https://github.com/rprichard/winpty/issues/118)

# Version 0.4.3 (2017-05-17)

Input handling changes:

 * winpty sets `ENHANCED_KEY` for arrow and navigation keys.  This fixes an
   issue with the Ruby REPL.
   [#99](https://github.com/rprichard/winpty/issues/99)
 * AltGr keys are handled better now.
   [#109](https://github.com/rprichard/winpty/issues/109)
 * In `ENABLE_VIRTUAL_TERMINAL_INPUT` mode, when typing Home/End with a
   modifier (e.g. Ctrl), winpty now generates an H/F escape sequence like
   `^[[1;5F` rather than a 1/4 escape like `^[[4;5~`.
   [#114](https://github.com/rprichard/winpty/issues/114)

Resizing and scraping fixes:

 * winpty now synthesizes a `WINDOW_BUFFER_SIZE_EVENT` event after resizing
   the console to better propagate window size changes to console programs.
   In particular, this affects WSL and Cygwin.
   [#110](https://github.com/rprichard/winpty/issues/110)
 * Better handling of resizing for certain full-screen programs, like
   WSL less.
   [#112](https://github.com/rprichard/winpty/issues/112)
 * Hide the cursor if it's currently outside the console window.  This change
   fixes an issue with Far Manager.
   [#113](https://github.com/rprichard/winpty/issues/113)
 * winpty now avoids using console fonts smaller than 5px high to improve
   half-vs-full-width character handling.  See
   https://github.com/Microsoft/vscode/issues/19665.
   [b4db322010](https://github.com/rprichard/winpty/commit/b4db322010d2d897e6c496fefc4f0ecc9b84c2f3)

Cygwin/MSYS adapter fix:

 * The way the `winpty` Cygwin/MSYS2 adapter searches for the program to
   launch changed.  It now resolves symlinks and searches the PATH explicitly.
   [#81](https://github.com/rprichard/winpty/issues/81)
   [#98](https://github.com/rprichard/winpty/issues/98)

This release does not include binaries for the old MSYS1 project anymore.
MSYS2 will continue to be supported.  See
https://github.com/rprichard/winpty/issues/97.

# Version 0.4.2 (2017-01-18)

This release improves WSL support (i.e. Bash-on-Windows):

 * winpty generates more correct input escape sequences for WSL programs that
   enable an alternate input mode using DECCKM.  This bug affected arrow keys
   and Home/End in WSL programs such as `vim`, `mc`, and `less`.
   [#90](https://github.com/rprichard/winpty/issues/90)
 * winpty now recognizes the `COMMON_LVB_REVERSE_VIDEO` and
   `COMMON_LVB_UNDERSCORE` text attributes.  The Windows console uses these
   attributes to implement the SGR.4(Underline) and SGR.7(Negative) modes in
   its VT handling.  This change affects WSL pager status bars, man pages, etc.

The build system no longer has a "version suffix" mechanism, so passing
`VERSION_SUFFIX=<suffix>` to make or `-D VERSION_SUFFIX=<suffix>` to gyp now
has no effect.  AFAIK, the mechanism was never used publicly.
[67a34b6c03](https://github.com/rprichard/winpty/commit/67a34b6c03557a5c2e0a2bdd502c2210921d8f3e)

# Version 0.4.1 (2017-01-03)

Bug fixes:

 * This version fixes a bug where the `winpty-agent.exe` process could read
   past the end of a buffer.
   [#94](https://github.com/rprichard/winpty/issues/94)

# Version 0.4.0 (2016-06-28)

The winpty library has a new API that should be easier for embedding.
[880c00c69e](https://github.com/rprichard/winpty/commit/880c00c69eeca73643ddb576f02c5badbec81f56)

User-visible changes:

 * winpty now automatically puts the terminal into mouse mode when it detects
   that the console has left QuickEdit mode.  The `--mouse` option still forces
   the terminal into mouse mode.  In principle, an option could be added to
   suppress terminal mode, but hopefully it won't be necessary.  There is a
   script in the `misc` subdirectory, `misc/ConinMode.ps1`, that can change
   the QuickEdit mode from the command-line.
 * winpty now passes keyboard escapes to `bash.exe` in the Windows Subsystem
   for Linux.
   [#82](https://github.com/rprichard/winpty/issues/82)

Bug fixes:

 * By default, `winpty.dll` avoids calling `SetProcessWindowStation` within
   the calling process.
   [#58](https://github.com/rprichard/winpty/issues/58)
 * Fixed an uninitialized memory bug that could have crashed winpty.
   [#80](https://github.com/rprichard/winpty/issues/80)
 * winpty now works better with very large and very small terminal windows.
   It resizes the console font according to the number of columns.
   [#61](https://github.com/rprichard/winpty/issues/61)
 * winpty no longer uses Mark to freeze the console on Windows 10.  The Mark
   command could interfere with the cursor position, corrupting the data in
   the screen buffer.
   [#79](https://github.com/rprichard/winpty/issues/79)

# Version 0.3.0 (2016-05-20)

User-visible changes:

 * The UNIX adapter is renamed from `console.exe` to `winpty.exe` to be
   consistent with MSYS2.  The name `winpty.exe` is less likely to conflict
   with another program and is easier to search for online (e.g. for someone
   unfamiliar with winpty).
 * The UNIX adapter now clears the `TERM` variable.
   [#43](https://github.com/rprichard/winpty/issues/43)
 * An escape character appearing in a console screen buffer cell is converted
   to a '?'.
   [#47](https://github.com/rprichard/winpty/issues/47)

Bug fixes:

 * A major bug affecting XP users was fixed.
   [#67](https://github.com/rprichard/winpty/issues/67)
 * Fixed an incompatibility with ConEmu where winpty hung if ConEmu's
   "Process 'start'" feature was enabled.
   [#70](https://github.com/rprichard/winpty/issues/70)
 * Fixed a bug where `cmd.exe` sometimes printed the message,
   `Not enough storage is available to process this command.`.
   [#74](https://github.com/rprichard/winpty/issues/74)

Many changes internally:

 * The codebase is switched from C++03 to C++11 and uses exceptions internally.
   No exceptions are thrown across the C APIs defined in `winpty.h`.
 * This version drops support for the original MinGW compiler packaged with
   Cygwin (`i686-pc-mingw32-g++`).  The MinGW-w64 compiler is still supported,
   as is the MinGW distributed at mingw.org.  Compiling with MSVC now requires
   MSVC 2013 or newer.  Windows XP is still supported.
   [ec3eae8df5](https://github.com/rprichard/winpty/commit/ec3eae8df5bbbb36d7628d168b0815638d122f37)
 * Pipe security is improved.  winpty works harder to produce unique pipe names
   and includes a random component in the name.  winpty secures pipes with a
   DACL that prevents arbitrary users from connecting to its pipes.  winpty now
   passes `PIPE_REJECT_REMOTE_CLIENTS` on Vista and up, and it verifies that
   the pipe client PID is correct, again on Vista and up.  When connecting to a
   named pipe, winpty uses the `SECURITY_IDENTIFICATION` flag to restrict
   impersonation.  Previous versions *should* still be secure.
 * `winpty-debugserver.exe` now has an `--everyone` flag that allows capturing
   debug output from other users.
 * The code now compiles cleanly with MSVC's "Security Development Lifecycle"
   (`/SDL`) checks enabled.

# Version 0.2.2 (2016-02-25)

Minor bug fixes and enhancements:

 * Fix a bug that generated spurious mouse input records when an incomplete
   mouse escape sequence was seen.
 * Fix a buffer overflow bug in `winpty-debugserver.exe` affecting messages of
   exactly 4096 bytes.
 * For MSVC builds, add a `src/configurations.gypi` file that can be included
   on the gyp command-line to enable 32-bit and 64-bit builds.
 * `winpty-agent --show-input` mode: Flush stdout after each line.
 * Makefile builds: generate a `build/winpty.lib` import library to accompany
   `build/winpty.dll`.

# Version 0.2.1 (2015-12-19)

 * The main project source was moved into a `src` directory for better code
   organization and to fix
   [#51](https://github.com/rprichard/winpty/issues/51).
 * winpty recognizes many more escape sequences, including:
    * putty/rxvt's F1-F4 keys
      [#40](https://github.com/rprichard/winpty/issues/40)
    * the Linux virtual console's F1-F5 keys
    * the "application numpad" keys (e.g. enabled with DECPAM)
 * Fixed handling of Shift-Alt-O and Alt-[.
 * Added support for mouse input.  The UNIX adapter has a `--mouse` argument
   that puts the terminal into mouse mode, but the agent recognizes mouse
   input even without the argument.  The agent recognizes double-clicks using
   Windows' double-click interval setting (i.e. GetDoubleClickTime).
   [#57](https://github.com/rprichard/winpty/issues/57)

Changes to debugging interfaces:

 * The `WINPTY_DEBUG` variable is now a comma-separated list.  The old
   behavior (i.e. tracing) is enabled with `WINPTY_DEBUG=trace`.
 * The UNIX adapter program now has a `--showkey` argument that dumps input
   bytes.
 * The `winpty-agent.exe` program has a `--show-input` argument that dumps
   `INPUT_RECORD` records.  (It omits mouse events unless `--with-mouse` is
   also specified.)  The agent also responds to `WINPTY_DEBUG=trace,input`,
   which logs input bytes and synthesized console events, and it responds to
   `WINPTY_DEBUG=trace,dump_input_map`, which dumps the internal table of
   escape sequences.

# Version 0.2.0 (2015-11-13)

No changes to the API, but many small changes to the implementation.  The big
changes include:

 * Support for 64-bit Cygwin and MSYS2
 * Support for Windows 10
 * Better Unicode support (especially East Asian languages)

Details:

 * The `configure` script recognizes 64-bit Cygwin and MSYS2 environments and
   selects the appropriate compiler.
 * winpty works much better with the upgraded console in Windows 10.  The
   `conhost.exe` hang can still occur, but only with certain programs, and
   is much less likely to occur.  With the new console, use Mark instead of
   SelectAll, for better performance.
   [#31](https://github.com/rprichard/winpty/issues/31)
   [#30](https://github.com/rprichard/winpty/issues/30)
   [#53](https://github.com/rprichard/winpty/issues/53)
 * The UNIX adapter now calls `setlocale(LC_ALL, "")` to set the locale.
 * Improved Unicode support.  When a console is started with an East Asian code
   page, winpty now chooses an East Asian font rather than Consolas / Lucida
   Console.  Selecting the right font helps synchronize character widths
   between the console and terminal.  (It's not perfect, though.)
   [#41](https://github.com/rprichard/winpty/issues/41)
 * winpty now more-or-less works with programs that change the screen buffer
   or resize the original screen buffer.  If the screen buffer height changes,
   winpty switches to a "direct mode", where it makes no effort to track
   scrolling.  In direct mode, it merely syncs snapshots of the console to the
   terminal.  Caveats:
    * Changing the screen buffer (i.e. `SetConsoleActiveScreenBuffer`)
      breaks winpty on Windows 7.  This problem can eventually be mitigated,
      but never completely fixed, due to Windows 7 bugginess.
    * Resizing the original screen buffer can hang `conhost.exe` on Windows 10.
      Enabling the legacy console is a workaround.
    * If a program changes the screen buffer and then exits, relying on the OS
      to restore the original screen buffer, that restoration probably will not
      happen with winpty.  winpty's behavior can probably be improved here.
 * Improved color handling:
    * DkGray-on-Black text was previously hiddenly completely.  Now it is
      output as DkGray, with a fallback to LtGray on terminals that don't
      recognize the intense colors.
      [#39](https://github.com/rprichard/winpty/issues/39).
    * The console is always initialized to LtGray-on-Black, regardless of the
      user setting, which matches the console color heuristic, which translates
      LtGray-on-Black to "reset SGR parameters."
 * Shift-Tab is recognized correctly now.
   [#19](https://github.com/rprichard/winpty/issues/19)
 * Add a `--version` argument to `winpty-agent.exe` and the UNIX adapter.  The
   argument reports the nominal version (i.e. the `VERSION.txt`) file, with a
   "VERSION_SUFFIX" appended (defaulted to `-dev`), and a git commit hash, if
   the `git` command successfully reports a hash during the build.  The `git`
   command is invoked by either `make` or `gyp`.
 * The agent now combines `ReadConsoleOutputW` calls when it polls the console
   buffer for changes, which may slightly reduce its CPU overhead.
   [#44](https://github.com/rprichard/winpty/issues/44).
 * A `gyp` file is added to help compile with MSVC.
 * The code can now be compiled as C++11 code, though it isn't by default.
   [bde8922e08](https://github.com/rprichard/winpty/commit/bde8922e08c3638e01ecc7b581b676c314163e3c)
 * If winpty can't create a new window station, it charges ahead rather than
   aborting.  This situation might happen if winpty were started from an SSH
   session.
 * Debugging improvements:
    * `WINPTYDBG` is renamed to `WINPTY_DEBUG`, and a new `WINPTY_SHOW_CONSOLE`
      variable keeps the underlying console visible.
    * A `winpty-debugserver.exe` program is built and shipped by default.  It
      collects the trace output enabled with `WINPTY_DEBUG`.
 * The `Makefile` build of winpty now compiles `winpty-agent.exe` and
   `winpty.dll` with -O2.

# Version 0.1.1 (2012-07-28)

Minor bugfix release.

# Version 0.1 (2012-04-17)

Initial release.
