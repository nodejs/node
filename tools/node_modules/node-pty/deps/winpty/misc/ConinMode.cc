#include <windows.h>

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>

#include <string>
#include <vector>

static HANDLE getConin() {
    HANDLE conin = GetStdHandle(STD_INPUT_HANDLE);
    if (conin == INVALID_HANDLE_VALUE) {
        fprintf(stderr, "error: cannot get stdin\n");
        exit(1);
    }
    return conin;
}

static DWORD getConsoleMode() {
    DWORD mode = 0;
    if (!GetConsoleMode(getConin(), &mode)) {
        fprintf(stderr, "error: GetConsoleMode failed (is stdin a console?)\n");
        exit(1);
    }
    return mode;
}

static void setConsoleMode(DWORD mode) {
    if (!SetConsoleMode(getConin(), mode)) {
        fprintf(stderr, "error: SetConsoleMode failed (is stdin a console?)\n");
        exit(1);
    }
}

static long parseInt(const std::string &s) {
    errno = 0;
    char *endptr = nullptr;
    long result = strtol(s.c_str(), &endptr, 0);
    if (errno != 0 || !endptr || *endptr != '\0') {
        fprintf(stderr, "error: could not parse integral argument '%s'\n", s.c_str());
        exit(1);
    }
    return result;
}

static void usage() {
    printf("Usage: ConinMode [verb] [options]\n");
    printf("Verbs:\n");
    printf("    [info]      Dumps info about mode flags.\n");
    printf("    get         Prints the mode DWORD.\n");
    printf("    set VALUE   Sets the mode to VALUE, which can be decimal, hex, or octal.\n");
    printf("    set VALUE MASK\n");
    printf("                Same as `set VALUE`, but only alters the bits in MASK.\n");
    exit(1);
}

struct {
    const char *name;
    DWORD value;
} kInputFlags[] = {
    "ENABLE_PROCESSED_INPUT",           ENABLE_PROCESSED_INPUT,         // 0x0001
    "ENABLE_LINE_INPUT",                ENABLE_LINE_INPUT,              // 0x0002
    "ENABLE_ECHO_INPUT",                ENABLE_ECHO_INPUT,              // 0x0004
    "ENABLE_WINDOW_INPUT",              ENABLE_WINDOW_INPUT,            // 0x0008
    "ENABLE_MOUSE_INPUT",               ENABLE_MOUSE_INPUT,             // 0x0010
    "ENABLE_INSERT_MODE",               ENABLE_INSERT_MODE,             // 0x0020
    "ENABLE_QUICK_EDIT_MODE",           ENABLE_QUICK_EDIT_MODE,         // 0x0040
    "ENABLE_EXTENDED_FLAGS",            ENABLE_EXTENDED_FLAGS,          // 0x0080
    "ENABLE_VIRTUAL_TERMINAL_INPUT", 0x0200/*ENABLE_VIRTUAL_TERMINAL_INPUT*/, // 0x0200
};

int main(int argc, char *argv[]) {
    std::vector<std::string> args;
    for (size_t i = 1; i < argc; ++i) {
        args.push_back(argv[i]);
    }

    if (args.empty() || args.size() == 1 && args[0] == "info") {
        DWORD mode = getConsoleMode();
        printf("mode: 0x%lx\n", mode);
        for (const auto &flag : kInputFlags) {
            printf("%-29s 0x%04lx %s\n", flag.name, flag.value, flag.value & mode ? "ON" : "off");
            mode &= ~flag.value;
        }
        for (int i = 0; i < 32; ++i) {
            if (mode & (1u << i)) {
                printf("Unrecognized flag: %04x\n", (1u << i));
            }
        }
        return 0;
    }

    const auto verb = args[0];

    if (verb == "set") {
        if (args.size() == 2) {
            const DWORD newMode = parseInt(args[1]);
            setConsoleMode(newMode);
        } else if (args.size() == 3) {
            const DWORD mode = parseInt(args[1]);
            const DWORD mask = parseInt(args[2]);
            const int newMode = (getConsoleMode() & ~mask) | (mode & mask);
            setConsoleMode(newMode);
        } else {
            usage();
        }
    } else if (verb == "get") {
        if (args.size() != 1) {
            usage();
        }
        printf("0x%lx\n", getConsoleMode());
    } else {
        usage();
    }

    return 0;
}
