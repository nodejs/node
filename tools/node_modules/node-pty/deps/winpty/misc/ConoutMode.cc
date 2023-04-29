#include <windows.h>

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>

#include <string>
#include <vector>

static HANDLE getConout() {
    HANDLE conout = GetStdHandle(STD_OUTPUT_HANDLE);
    if (conout == INVALID_HANDLE_VALUE) {
        fprintf(stderr, "error: cannot get stdout\n");
        exit(1);
    }
    return conout;
}

static DWORD getConsoleMode() {
    DWORD mode = 0;
    if (!GetConsoleMode(getConout(), &mode)) {
        fprintf(stderr, "error: GetConsoleMode failed (is stdout a console?)\n");
        exit(1);
    }
    return mode;
}

static void setConsoleMode(DWORD mode) {
    if (!SetConsoleMode(getConout(), mode)) {
        fprintf(stderr, "error: SetConsoleMode failed (is stdout a console?)\n");
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
    printf("Usage: ConoutMode [verb] [options]\n");
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
} kOutputFlags[] = {
    "ENABLE_PROCESSED_OUTPUT",          ENABLE_PROCESSED_OUTPUT,        // 0x0001
    "ENABLE_WRAP_AT_EOL_OUTPUT",        ENABLE_WRAP_AT_EOL_OUTPUT,      // 0x0002
    "ENABLE_VIRTUAL_TERMINAL_PROCESSING", 0x0004/*ENABLE_VIRTUAL_TERMINAL_PROCESSING*/, // 0x0004
    "DISABLE_NEWLINE_AUTO_RETURN",      0x0008/*DISABLE_NEWLINE_AUTO_RETURN*/, // 0x0008
    "ENABLE_LVB_GRID_WORLDWIDE",        0x0010/*ENABLE_LVB_GRID_WORLDWIDE*/, //0x0010
};

int main(int argc, char *argv[]) {
    std::vector<std::string> args;
    for (size_t i = 1; i < argc; ++i) {
        args.push_back(argv[i]);
    }

    if (args.empty() || args.size() == 1 && args[0] == "info") {
        DWORD mode = getConsoleMode();
        printf("mode: 0x%lx\n", mode);
        for (const auto &flag : kOutputFlags) {
            printf("%-34s 0x%04lx %s\n", flag.name, flag.value, flag.value & mode ? "ON" : "off");
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
