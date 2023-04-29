// Copyright (c) 2011-2015 Ryan Prichard
//
// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files (the "Software"), to
// deal in the Software without restriction, including without limitation the
// rights to use, copy, modify, merge, publish, distribute, sublicense, and/or
// sell copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions:
//
// The above copyright notice and this permission notice shall be included in
// all copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
// FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS
// IN THE SOFTWARE.

// MSYS's sys/cygwin.h header only declares cygwin_internal if WINVER is
// defined, which is defined in windows.h.  Therefore, include windows.h early.
#include <windows.h>

#include <assert.h>
#include <cygwin/version.h>
#include <errno.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/select.h>
#include <sys/cygwin.h>
#include <termios.h>
#include <unistd.h>

#include <map>
#include <string>
#include <utility>
#include <vector>

#include <winpty.h>
#include "../shared/DebugClient.h"
#include "../shared/UnixCtrlChars.h"
#include "../shared/WinptyVersion.h"
#include "InputHandler.h"
#include "OutputHandler.h"
#include "Util.h"
#include "WakeupFd.h"

#define CSI "\x1b["

static WakeupFd *g_mainWakeup = NULL;

static WakeupFd &mainWakeup()
{
    if (g_mainWakeup == NULL) {
        static const char msg[] = "Internal error: g_mainWakeup is NULL\r\n";
        write(STDERR_FILENO, msg, sizeof(msg) - 1);
        abort();
    }
    return *g_mainWakeup;
}

struct SavedTermiosMode {
    int count;
    bool valid[3];
    termios mode[3];
};

// Put the input terminal into non-canonical mode.
static SavedTermiosMode setRawTerminalMode(
    bool allowNonTtys, bool setStdout, bool setStderr)
{
    SavedTermiosMode ret;
    const char *const kNames[3] = { "stdin", "stdout", "stderr" };

    ret.valid[0] = true;
    ret.valid[1] = setStdout;
    ret.valid[2] = setStderr;

    for (int i = 0; i < 3; ++i) {
        if (!ret.valid[i]) {
            continue;
        }
        if (!isatty(i)) {
            ret.valid[i] = false;
            if (!allowNonTtys) {
                fprintf(stderr, "%s is not a tty\n", kNames[i]);
                exit(1);
            }
        } else {
            ret.valid[i] = true;
            if (tcgetattr(i, &ret.mode[i]) < 0) {
                perror("tcgetattr failed");
                exit(1);
            }
        }
    }

    if (ret.valid[STDIN_FILENO]) {
        termios buf;
        if (tcgetattr(STDIN_FILENO, &buf) < 0) {
            perror("tcgetattr failed");
            exit(1);
        }
        buf.c_lflag &= ~(ECHO | ICANON | IEXTEN | ISIG);
        buf.c_iflag &= ~(BRKINT | ICRNL | INPCK | ISTRIP | IXON);
        buf.c_cflag &= ~(CSIZE | PARENB);
        buf.c_cflag |= CS8;
        buf.c_cc[VMIN] = 1;  // blocking read
        buf.c_cc[VTIME] = 0;
        if (tcsetattr(STDIN_FILENO, TCSAFLUSH, &buf) < 0) {
            fprintf(stderr, "tcsetattr failed\n");
            exit(1);
        }
    }

    for (int i = STDOUT_FILENO; i <= STDERR_FILENO; ++i) {
        if (!ret.valid[i]) {
            continue;
        }
        termios buf;
        if (tcgetattr(i, &buf) < 0) {
            perror("tcgetattr failed");
            exit(1);
        }
        buf.c_cflag &= ~(CSIZE | PARENB);
        buf.c_cflag |= CS8;
        buf.c_oflag &= ~OPOST;
        if (tcsetattr(i, TCSAFLUSH, &buf) < 0) {
            fprintf(stderr, "tcsetattr failed\n");
            exit(1);
        }
    }

    return ret;
}

static void restoreTerminalMode(const SavedTermiosMode &original)
{
    for (int i = 0; i < 3; ++i) {
        if (!original.valid[i]) {
            continue;
        }
        if (tcsetattr(i, TCSAFLUSH, &original.mode[i]) < 0) {
            perror("error restoring terminal mode");
            exit(1);
        }
    }
}

static void debugShowKey(bool allowNonTtys)
{
    printf("\nPress any keys -- Ctrl-D exits\n\n");
    const SavedTermiosMode saved =
        setRawTerminalMode(allowNonTtys, false, false);
    char buf[128];
    while (true) {
        const ssize_t len = read(STDIN_FILENO, buf, sizeof(buf));
        if (len <= 0) {
            break;
        }
        for (int i = 0; i < len; ++i) {
            char ctrl = decodeUnixCtrlChar(buf[i]);
            if (ctrl == '\0') {
                putchar(buf[i]);
            } else {
                putchar('^');
                putchar(ctrl);
            }
        }
        for (int i = 0; i < len; ++i) {
            unsigned char uch = buf[i];
            printf("\t%3d %04o 0x%02x\n", uch, uch, uch);
            fflush(stdout);
        }
        if (buf[0] == 4) {
            // Ctrl-D
            break;
        }
    }
    restoreTerminalMode(saved);
}

static void terminalResized(int signo)
{
    mainWakeup().set();
}

static void registerResizeSignalHandler()
{
    struct sigaction resizeSigAct;
    memset(&resizeSigAct, 0, sizeof(resizeSigAct));
    resizeSigAct.sa_handler = terminalResized;
    resizeSigAct.sa_flags = SA_RESTART;
    sigaction(SIGWINCH, &resizeSigAct, NULL);
}

// Convert the path to a Win32 path if it is a POSIX path, and convert slashes
// to backslashes.
static std::string convertPosixPathToWin(const std::string &path)
{
    char *tmp;
#if defined(CYGWIN_VERSION_CYGWIN_CONV) && \
        CYGWIN_VERSION_API_MINOR >= CYGWIN_VERSION_CYGWIN_CONV
    // MSYS2 and versions of Cygwin released after 2009 or so use this API.
    // The original MSYS still lacks this API.
    ssize_t newSize = cygwin_conv_path(CCP_POSIX_TO_WIN_A | CCP_ABSOLUTE,
                                       path.c_str(), NULL, 0);
    assert(newSize >= 0);
    tmp = new char[newSize + 1];
    ssize_t success = cygwin_conv_path(CCP_POSIX_TO_WIN_A | CCP_ABSOLUTE,
                                       path.c_str(), tmp, newSize + 1);
    assert(success == 0);
#else
    // In the current Cygwin header file, this API is documented as deprecated
    // because it's restricted to paths of MAX_PATH length.  In the CVS version
    // of MSYS, the newer API doesn't exist, and this older API is implemented
    // using msys_p2w, which seems like it would handle paths larger than
    // MAX_PATH, but there's no way to query how large the new path is.
    // Hopefully, this is large enough.
    tmp = new char[MAX_PATH + path.size()];
    cygwin_conv_to_win32_path(path.c_str(), tmp);
#endif
    for (int i = 0; tmp[i] != '\0'; ++i) {
        if (tmp[i] == '/')
            tmp[i] = '\\';
    }
    std::string ret(tmp);
    delete [] tmp;
    return ret;
}

static std::string resolvePath(const std::string &path)
{
    char ret[PATH_MAX];
    ret[0] = '\0';
    if (realpath(path.c_str(), ret) != ret) {
        return std::string();
    }
    return ret;
}

template <size_t N>
static bool endsWith(const std::string &path, const char (&suf)[N])
{
    const size_t suffixLen = N - 1;
    char actualSuf[N];
    if (path.size() < suffixLen) {
        return false;
    }
    strcpy(actualSuf, &path.c_str()[path.size() - suffixLen]);
    for (size_t i = 0; i < suffixLen; ++i) {
        actualSuf[i] = tolower(actualSuf[i]);
    }
    return !strcmp(actualSuf, suf);
}

static std::string findProgram(
    const char *winptyProgName,
    const std::string &prog)
{
    std::string candidate;
    if (prog.find('/') == std::string::npos &&
            prog.find('\\') == std::string::npos) {
        // XXX: It would be nice to use a lambda here (once/if old MSYS support
        // is dropped).
        // Search the PATH.
        const char *const pathVar = getenv("PATH");
        const std::string pathList(pathVar ? pathVar : "");
        size_t elpos = 0;
        while (true) {
            const size_t elend = pathList.find(':', elpos);
            candidate = pathList.substr(elpos, elend - elpos);
            if (!candidate.empty() && *(candidate.end() - 1) != '/') {
                candidate += '/';
            }
            candidate += prog;
            candidate = resolvePath(candidate);
            if (!candidate.empty()) {
                int perm = X_OK;
                if (endsWith(candidate, ".bat") || endsWith(candidate, ".cmd")) {
#ifdef __MSYS__
                    // In MSYS/MSYS2, batch files don't have the execute bit
                    // set, so just check that they're readable.
                    perm = R_OK;
#endif
                } else if (endsWith(candidate, ".com") || endsWith(candidate, ".exe")) {
                    // Do nothing.
                } else {
                    // Make the exe extension explicit so that we don't try to
                    // run shell scripts with CreateProcess/winpty_spawn.
                    candidate += ".exe";
                }
                if (!access(candidate.c_str(), perm)) {
                    break;
                }
            }
            if (elend == std::string::npos) {
                fprintf(stderr, "%s: error: cannot start '%s': Not found in PATH\n",
                    winptyProgName, prog.c_str());
                exit(1);
            } else {
                elpos = elend + 1;
            }
        }
    } else {
        candidate = resolvePath(prog);
        if (candidate.empty()) {
            std::string errstr(strerror(errno));
            fprintf(stderr, "%s: error: cannot start '%s': %s\n",
                winptyProgName, prog.c_str(), errstr.c_str());
            exit(1);
        }
    }
    return convertPosixPathToWin(candidate);
}

// Convert argc/argv into a Win32 command-line following the escaping convention
// documented on MSDN.  (e.g. see CommandLineToArgvW documentation)
static std::string argvToCommandLine(const std::vector<std::string> &argv)
{
    std::string result;
    for (size_t argIndex = 0; argIndex < argv.size(); ++argIndex) {
        if (argIndex > 0)
            result.push_back(' ');
        const char *arg = argv[argIndex].c_str();
        const bool quote =
            strchr(arg, ' ') != NULL ||
            strchr(arg, '\t') != NULL ||
            *arg == '\0';
        if (quote)
            result.push_back('\"');
        int bsCount = 0;
        for (const char *p = arg; *p != '\0'; ++p) {
            if (*p == '\\') {
                bsCount++;
            } else if (*p == '\"') {
                result.append(bsCount * 2 + 1, '\\');
                result.push_back('\"');
                bsCount = 0;
            } else {
                result.append(bsCount, '\\');
                bsCount = 0;
                result.push_back(*p);
            }
        }
        if (quote) {
            result.append(bsCount * 2, '\\');
            result.push_back('\"');
        } else {
            result.append(bsCount, '\\');
        }
    }
    return result;
}

static wchar_t *heapMbsToWcs(const char *text)
{
    // Calling mbstowcs with a NULL first argument seems to be broken on MSYS.
    // Instead of returning the size of the converted string, it returns 0.
    // Using strlen(text) * 2 is probably big enough.
    size_t maxLen = strlen(text) * 2 + 1;
    wchar_t *ret = new wchar_t[maxLen];
    size_t len = mbstowcs(ret, text, maxLen);
    assert(len != (size_t)-1 && len < maxLen);
    return ret;
}

static char *heapWcsToMbs(const wchar_t *text)
{
    // Calling wcstombs with a NULL first argument seems to be broken on MSYS.
    // Instead of returning the size of the converted string, it returns 0.
    // Using wcslen(text) * 3 is big enough for UTF-8 and probably other
    // encodings.  For UTF-8, codepoints that fit in a single wchar
    // (U+0000 to U+FFFF) are encoded using 1-3 bytes.  The remaining code
    // points needs two wchar's and are encoded using 4 bytes.
    size_t maxLen = wcslen(text) * 3 + 1;
    char *ret = new char[maxLen];
    size_t len = wcstombs(ret, text, maxLen);
    if (len == (size_t)-1 || len >= maxLen) {
        delete [] ret;
        return NULL;
    } else {
        return ret;
    }
}

static std::string wcsToMbs(const wchar_t *text)
{
    std::string ret;
    const char *ptr = heapWcsToMbs(text);
    if (ptr != NULL) {
        ret = ptr;
        delete [] ptr;
    }
    return ret;
}

void setupWin32Environment()
{
    std::map<std::string, std::string> varsToCopy;
    const char *vars[] = {
        "WINPTY_DEBUG",
        "WINPTY_SHOW_CONSOLE",
        NULL
    };
    for (int i = 0; vars[i] != NULL; ++i) {
        const char *cstr = getenv(vars[i]);
        if (cstr != NULL && cstr[0] != '\0') {
            varsToCopy[vars[i]] = cstr;
        }
    }

#if defined(__MSYS__) && CYGWIN_VERSION_API_MINOR >= 48 || \
        !defined(__MSYS__) && CYGWIN_VERSION_API_MINOR >= 153
    // Use CW_SYNC_WINENV to copy the Unix environment to the Win32
    // environment.  The command performs special translation on some variables
    // (such as PATH and TMP).  It also copies the debugging environment
    // variables.
    //
    // Note that the API minor versions have diverged in Cygwin and MSYS.
    // CW_SYNC_WINENV was added to Cygwin in version 153.  (Cygwin's
    // include/cygwin/version.h says that CW_SETUP_WINENV was added in 153.
    // The flag was renamed 8 days after it was added, but the API docs weren't
    // updated.)  The flag was added to MSYS in version 48.
    //
    // Also, in my limited testing, this call seems to be necessary with Cygwin
    // but unnecessary with MSYS.  Perhaps MSYS is automatically syncing the
    // Unix environment with the Win32 environment before starting console.exe?
    // It shouldn't hurt to call it for MSYS.
    cygwin_internal(CW_SYNC_WINENV);
#endif

    // Copy debugging environment variables from the Cygwin environment
    // to the Win32 environment so the agent will inherit it.
    for (std::map<std::string, std::string>::iterator it = varsToCopy.begin();
            it != varsToCopy.end();
            ++it) {
        wchar_t *nameW = heapMbsToWcs(it->first.c_str());
        wchar_t *valueW = heapMbsToWcs(it->second.c_str());
        SetEnvironmentVariableW(nameW, valueW);
        delete [] nameW;
        delete [] valueW;
    }

    // Clear the TERM variable.  The child process's immediate console/terminal
    // environment is a Windows console, not the terminal that winpty is
    // communicating with.  Leaving the TERM variable set can break programs in
    // various ways.  (e.g. arrows keys broken in Cygwin less, IronPython's
    // help(...) function doesn't start, misc programs decide they should
    // output color escape codes on pre-Win10).  See
    // https://github.com/rprichard/winpty/issues/43.
    SetEnvironmentVariableW(L"TERM", NULL);
}

static void usage(const char *program, int exitCode)
{
    printf("Usage: %s [options] [--] program [args]\n", program);
    printf("\n");
    printf("Options:\n");
    printf("  -h, --help  Show this help message\n");
    printf("  --mouse     Enable terminal mouse input\n");
    printf("  --showkey   Dump STDIN escape sequences\n");
    printf("  --version   Show the winpty version number\n");
    exit(exitCode);
}

struct Arguments {
    std::vector<std::string> childArgv;
    bool mouseInput;
    bool testAllowNonTtys;
    bool testConerr;
    bool testPlainOutput;
    bool testColorEscapes;
};

static void parseArguments(int argc, char *argv[], Arguments &out)
{
    out.mouseInput = false;
    out.testAllowNonTtys = false;
    out.testConerr = false;
    out.testPlainOutput = false;
    out.testColorEscapes = false;
    bool doShowKeys = false;
    const char *const program = argc >= 1 ? argv[0] : "<program>";
    int argi = 1;
    while (argi < argc) {
        std::string arg(argv[argi++]);
        if (arg.size() >= 1 && arg[0] == '-') {
            if (arg == "-h" || arg == "--help") {
                usage(program, 0);
            } else if (arg == "--mouse") {
                out.mouseInput = true;
            } else if (arg == "--showkey") {
                doShowKeys = true;
            } else if (arg == "--version") {
                dumpVersionToStdout();
                exit(0);
            } else if (arg == "-Xallow-non-tty") {
                out.testAllowNonTtys = true;
            } else if (arg == "-Xconerr") {
                out.testConerr = true;
            } else if (arg == "-Xplain") {
                out.testPlainOutput = true;
            } else if (arg == "-Xcolor") {
                out.testColorEscapes = true;
            } else if (arg == "--") {
                break;
            } else {
                fprintf(stderr, "Error: unrecognized option: '%s'\n",
                    arg.c_str());
                exit(1);
            }
        } else {
            out.childArgv.push_back(arg);
            break;
        }
    }
    for (; argi < argc; ++argi) {
        out.childArgv.push_back(argv[argi]);
    }
    if (doShowKeys) {
        debugShowKey(out.testAllowNonTtys);
        exit(0);
    }
    if (out.childArgv.size() == 0) {
        usage(program, 1);
    }
}

static std::string errorMessageToString(DWORD err)
{
    // Use FormatMessageW rather than FormatMessageA, because we want to use
    // wcstombs to convert to the Cygwin locale, which might not match the
    // codepage FormatMessageA would use.  We need to convert using wcstombs,
    // rather than print using %ls, because %ls doesn't work in the original
    // MSYS.
    wchar_t *wideMsgPtr = NULL;
    const DWORD formatRet = FormatMessageW(
        FORMAT_MESSAGE_FROM_SYSTEM |
            FORMAT_MESSAGE_ALLOCATE_BUFFER |
            FORMAT_MESSAGE_IGNORE_INSERTS,
        NULL,
        err,
        MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
        reinterpret_cast<wchar_t*>(&wideMsgPtr),
        0,
        NULL);
    if (formatRet == 0 || wideMsgPtr == NULL) {
        return std::string();
    }
    std::string msg = wcsToMbs(wideMsgPtr);
    LocalFree(wideMsgPtr);
    const size_t pos = msg.find_last_not_of(" \r\n\t");
    if (pos == std::string::npos) {
        msg.clear();
    } else {
        msg.erase(pos + 1);
    }
    return msg;
}

static std::string formatErrorMessage(DWORD err)
{
    char buf[64];
    sprintf(buf, "error %#x", static_cast<unsigned int>(err));
    std::string ret = errorMessageToString(err);
    if (ret.empty()) {
        ret += buf;
    } else {
        ret += " (";
        ret += buf;
        ret += ")";
    }
    return ret;
}

int main(int argc, char *argv[])
{
    setlocale(LC_ALL, "");

    g_mainWakeup = new WakeupFd();

    Arguments args;
    parseArguments(argc, argv, args);

    setupWin32Environment();

    winsize sz = { 0 };
    sz.ws_col = 80;
    sz.ws_row = 25;
    ioctl(STDIN_FILENO, TIOCGWINSZ, &sz);

    DWORD agentFlags = WINPTY_FLAG_ALLOW_CURPROC_DESKTOP_CREATION;
    if (args.testConerr)        { agentFlags |= WINPTY_FLAG_CONERR; }
    if (args.testPlainOutput)   { agentFlags |= WINPTY_FLAG_PLAIN_OUTPUT; }
    if (args.testColorEscapes)  { agentFlags |= WINPTY_FLAG_COLOR_ESCAPES; }
    winpty_config_t *agentCfg = winpty_config_new(agentFlags, NULL);
    assert(agentCfg != NULL);
    winpty_config_set_initial_size(agentCfg, sz.ws_col, sz.ws_row);
    if (args.mouseInput) {
        winpty_config_set_mouse_mode(agentCfg, WINPTY_MOUSE_MODE_FORCE);
    }

    winpty_error_ptr_t openErr = NULL;
    winpty_t *wp = winpty_open(agentCfg, &openErr);
    if (wp == NULL) {
        fprintf(stderr, "Error creating winpty: %s\n",
            wcsToMbs(winpty_error_msg(openErr)).c_str());
        exit(1);
    }
    winpty_config_free(agentCfg);
    winpty_error_free(openErr);

    HANDLE conin = CreateFileW(winpty_conin_name(wp), GENERIC_WRITE, 0, NULL,
                               OPEN_EXISTING, 0, NULL);
    HANDLE conout = CreateFileW(winpty_conout_name(wp), GENERIC_READ, 0, NULL,
                                OPEN_EXISTING, 0, NULL);
    assert(conin != INVALID_HANDLE_VALUE);
    assert(conout != INVALID_HANDLE_VALUE);
    HANDLE conerr = NULL;
    if (args.testConerr) {
        conerr = CreateFileW(winpty_conerr_name(wp), GENERIC_READ, 0, NULL,
                             OPEN_EXISTING, 0, NULL);
        assert(conerr != INVALID_HANDLE_VALUE);
    }

    HANDLE childHandle = NULL;

    {
        // Start the child process under the console.
        args.childArgv[0] = findProgram(argv[0], args.childArgv[0]);
        std::string cmdLine = argvToCommandLine(args.childArgv);
        wchar_t *cmdLineW = heapMbsToWcs(cmdLine.c_str());

        winpty_spawn_config_t *spawnCfg = winpty_spawn_config_new(
                WINPTY_SPAWN_FLAG_AUTO_SHUTDOWN,
                NULL, cmdLineW, NULL, NULL, NULL);
        assert(spawnCfg != NULL);

        winpty_error_ptr_t spawnErr = NULL;
        DWORD lastError = 0;
        BOOL spawnRet = winpty_spawn(wp, spawnCfg, &childHandle, NULL,
            &lastError, &spawnErr);
        winpty_spawn_config_free(spawnCfg);

        if (!spawnRet) {
            winpty_result_t spawnCode = winpty_error_code(spawnErr);
            if (spawnCode == WINPTY_ERROR_SPAWN_CREATE_PROCESS_FAILED) {
                fprintf(stderr, "%s: error: cannot start '%s': %s\n",
                    argv[0],
                    cmdLine.c_str(),
                    formatErrorMessage(lastError).c_str());
            } else {
                fprintf(stderr, "%s: error: cannot start '%s': internal error: %s\n",
                    argv[0],
                    cmdLine.c_str(),
                    wcsToMbs(winpty_error_msg(spawnErr)).c_str());
            }
            exit(1);
        }
        winpty_error_free(spawnErr);
        delete [] cmdLineW;
    }

    registerResizeSignalHandler();
    SavedTermiosMode mode =
        setRawTerminalMode(args.testAllowNonTtys, true, args.testConerr);

    InputHandler inputHandler(conin, STDIN_FILENO, mainWakeup());
    OutputHandler outputHandler(conout, STDOUT_FILENO, mainWakeup());
    OutputHandler *errorHandler = NULL;
    if (args.testConerr) {
        errorHandler = new OutputHandler(conerr, STDERR_FILENO, mainWakeup());
    }

    while (true) {
        fd_set readfds;
        FD_ZERO(&readfds);
        FD_SET(mainWakeup().fd(), &readfds);
        selectWrapper("main thread", mainWakeup().fd() + 1, &readfds);
        mainWakeup().reset();

        // Check for terminal resize.
        {
            winsize sz2;
            ioctl(STDIN_FILENO, TIOCGWINSZ, &sz2);
            if (memcmp(&sz, &sz2, sizeof(sz)) != 0) {
                sz = sz2;
                winpty_set_size(wp, sz.ws_col, sz.ws_row, NULL);
            }
        }

        // Check for an I/O handler shutting down (possibly indicating that the
        // child process has exited).
        if (inputHandler.isComplete() || outputHandler.isComplete() ||
                (errorHandler != NULL && errorHandler->isComplete())) {
            break;
        }
    }

    // Kill the agent connection.  This will kill the agent, closing the CONIN
    // and CONOUT pipes on the agent pipe, prompting our I/O handler to shut
    // down.
    winpty_free(wp);

    inputHandler.shutdown();
    outputHandler.shutdown();
    CloseHandle(conin);
    CloseHandle(conout);

    if (errorHandler != NULL) {
        errorHandler->shutdown();
        delete errorHandler;
        CloseHandle(conerr);
    }

    restoreTerminalMode(mode);

    DWORD exitCode = 0;
    if (!GetExitCodeProcess(childHandle, &exitCode)) {
        exitCode = 1;
    }
    CloseHandle(childHandle);
    return exitCode;
}
