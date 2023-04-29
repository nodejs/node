#include <windows.h>

#include <assert.h>
#include <stdint.h>
#include <stdlib.h>

#include <string>
#include <vector>

static std::wstring mbsToWcs(const std::string &s) {
    const size_t len = mbstowcs(nullptr, s.c_str(), 0);
    if (len == static_cast<size_t>(-1)) {
        assert(false && "mbsToWcs: invalid string");
    }
    std::wstring ret;
    ret.resize(len);
    const size_t len2 = mbstowcs(&ret[0], s.c_str(), len);
    assert(len == len2);
    return ret;
}

uint32_t parseHex(wchar_t ch, bool &invalid) {
    if (ch >= L'0' && ch <= L'9') {
        return ch - L'0';
    } else if (ch >= L'a' && ch <= L'f') {
        return ch - L'a' + 10;
    } else if (ch >= L'A' && ch <= L'F') {
        return ch - L'A' + 10;
    } else {
        invalid = true;
        return 0;
    }
}

int main(int argc, char *argv[]) {
    std::vector<std::wstring> args;
    for (int i = 1; i < argc; ++i) {
        args.push_back(mbsToWcs(argv[i]));
    }

    std::wstring out;
    for (const auto &arg : args) {
        if (!out.empty()) {
            out.push_back(L' ');
        }
        for (size_t i = 0; i < arg.size(); ++i) {
            wchar_t ch = arg[i];
            wchar_t nch = i + 1 < arg.size() ? arg[i + 1] : L'\0';
            if (ch == L'\\') {
                switch (nch) {
                    case L'a':  ch = L'\a'; ++i; break;
                    case L'b':  ch = L'\b'; ++i; break;
                    case L'e':  ch = L'\x1b'; ++i; break;
                    case L'f':  ch = L'\f'; ++i; break;
                    case L'n':  ch = L'\n'; ++i; break;
                    case L'r':  ch = L'\r'; ++i; break;
                    case L't':  ch = L'\t'; ++i; break;
                    case L'v':  ch = L'\v'; ++i; break;
                    case L'\\': ch = L'\\'; ++i; break;
                    case L'\'': ch = L'\''; ++i; break;
                    case L'\"': ch = L'\"'; ++i; break;
                    case L'\?': ch = L'\?'; ++i; break;
                    case L'x':
                        if (i + 3 < arg.size()) {
                            bool invalid = false;
                            uint32_t d1 = parseHex(arg[i + 2], invalid);
                            uint32_t d2 = parseHex(arg[i + 3], invalid);
                            if (!invalid) {
                                i += 3;
                                ch = (d1 << 4) | d2;
                            }
                        }
                        break;
                    case L'u':
                        if (i + 5 < arg.size()) {
                            bool invalid = false;
                            uint32_t d1 = parseHex(arg[i + 2], invalid);
                            uint32_t d2 = parseHex(arg[i + 3], invalid);
                            uint32_t d3 = parseHex(arg[i + 4], invalid);
                            uint32_t d4 = parseHex(arg[i + 5], invalid);
                            if (!invalid) {
                                i += 5;
                                ch = (d1 << 24) | (d2 << 16) | (d3 << 8) | d4;
                            }
                        }
                        break;
                    default: break;
                }
            }
            out.push_back(ch);
        }
    }

    DWORD actual = 0;
    if (!WriteConsoleW(
            GetStdHandle(STD_OUTPUT_HANDLE),
            out.c_str(),
            out.size(),
            &actual,
            nullptr)) {
        fprintf(stderr, "WriteConsole failed (is stdout a console?)\n");
        exit(1);
    }

    return 0;
}
