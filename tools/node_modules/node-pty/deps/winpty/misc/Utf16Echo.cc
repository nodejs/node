#include <windows.h>
#include <assert.h>
#include <stdlib.h>
#include <stdio.h>

#include <vector>
#include <string>

int main(int argc, char *argv[]) {
    system("cls");

    if (argc == 1) {
        printf("Usage: %s hhhh\n", argv[0]);
        return 0;
    }

    std::wstring dataToWrite;
    for (int i = 1; i < argc; ++i) {
        wchar_t ch = strtol(argv[i], NULL, 16);
        dataToWrite.push_back(ch);
    }

    DWORD actual = 0;
    BOOL ret = WriteConsoleW(
        GetStdHandle(STD_OUTPUT_HANDLE),
        dataToWrite.data(), dataToWrite.size(), &actual, NULL);
    assert(ret && actual == dataToWrite.size());

    // Read it back.
    std::vector<CHAR_INFO> readBuffer(dataToWrite.size() * 2);
    COORD bufSize = {static_cast<short>(readBuffer.size()), 1};
    COORD bufCoord = {0, 0};
    SMALL_RECT topLeft = {0, 0, static_cast<short>(readBuffer.size() - 1), 0};
    ret = ReadConsoleOutputW(
            GetStdHandle(STD_OUTPUT_HANDLE), readBuffer.data(),
            bufSize, bufCoord, &topLeft);
    assert(ret);

    printf("\n");
    for (int i = 0; i < readBuffer.size(); ++i) {
        printf("CHAR: %04x %04x\n",
            readBuffer[i].Char.UnicodeChar,
            readBuffer[i].Attributes);
    }
    return 0;
}
