// This test program is useful for studying commandline<->argv conversion.

#include <stdio.h>
#include <windows.h>

int main(int argc, char **argv)
{
    printf("cmdline = [%s]\n", GetCommandLine());
    for (int i = 0; i < argc; ++i)
        printf("[%s]\n", argv[i]);
    return 0;
}
