#include "utf8_args.h"

#ifdef _WIN32

#include <malloc.h>
#include <shellapi.h>
#include <windows.h>

void GetUtf8CommandLineArgs(int* argc, char*** argv) {
  int wargc;
  wchar_t** wargv = CommandLineToArgvW(GetCommandLineW(), &wargc);
  if (wargv == NULL) {
    *argc = 0;
    *argv = NULL;
    return;
  }

  *argc = wargc;
  *argv = (char**)malloc(wargc * sizeof(char*));
  for (int i = 0; i < wargc; ++i) {
    int len =
        WideCharToMultiByte(CP_UTF8, 0, wargv[i], -1, NULL, 0, NULL, NULL);
    (*argv)[i] = malloc(len);
    WideCharToMultiByte(CP_UTF8, 0, wargv[i], -1, (*argv)[i], len, NULL, NULL);
  }

  LocalFree(wargv);
}

void FreeUtf8CommandLineArgs(int argc, char** argv) {
  for (int i = 0; i < argc; ++i) {
    free(argv[i]);
  }
  free(argv);
}

#else

void GetUtf8CommandLineArgs(int* /*argc*/, char*** /*argv*/) {
  // Do nothing on non-Windows platforms.
}

void FreeUtf8CommandLineArgs(int /*argc*/, char** /*argv*/) {
  // Do nothing on non-Windows platforms.
}

#endif
