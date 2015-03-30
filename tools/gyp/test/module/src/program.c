#include <stdio.h>
#include <stdlib.h>

#if defined(PLATFORM_WIN)
#include <windows.h>
#elif defined(PLATFORM_MAC) || defined(PLATFORM_LINUX)
#include <dlfcn.h>
#include <libgen.h>
#include <string.h>
#include <sys/param.h>
#define MAX_PATH PATH_MAX
#endif

#if defined(PLATFORM_WIN)
#define MODULE_SUFFIX ".dll"
#elif defined(PLATFORM_MAC)
#define MODULE_SUFFIX ".so"
#elif defined(PLATFORM_LINUX)
#define MODULE_SUFFIX ".so"
#endif

typedef void (*module_symbol)(void);
char bin_path[MAX_PATH + 1];


void CallModule(const char* module) {
  char module_path[MAX_PATH + 1];
  const char* module_function = "module_main";
  module_symbol funcptr;
#if defined(PLATFORM_WIN)
  HMODULE dl;
  char drive[_MAX_DRIVE];
  char dir[_MAX_DIR];

  if (_splitpath_s(bin_path, drive, _MAX_DRIVE, dir, _MAX_DIR,
                    NULL, 0, NULL, 0)) {
    fprintf(stderr, "Failed to split executable path.\n");
    return;
  }
  if (_makepath_s(module_path, MAX_PATH, drive, dir, module, MODULE_SUFFIX)) {
    fprintf(stderr, "Failed to calculate module path.\n");
    return;
  }

  dl = LoadLibrary(module_path);
  if (!dl) {
    fprintf(stderr, "Failed to open module: %s\n", module_path);
    return;
  }

  funcptr = (module_symbol) GetProcAddress(dl, module_function);
  if (!funcptr) {
    fprintf(stderr, "Failed to find symbol: %s\n", module_function);
    return;
  }
  funcptr();

  FreeLibrary(dl);
#elif defined(PLATFORM_MAC) || defined(PLATFORM_LINUX)
  void* dl;
  char* path_copy = strdup(bin_path);
  char* bin_dir = dirname(path_copy);
  int path_size = snprintf(module_path, MAX_PATH, "%s/%s%s", bin_dir, module,
                           MODULE_SUFFIX);
  free(path_copy);
  if (path_size < 0 || path_size > MAX_PATH) {
    fprintf(stderr, "Failed to calculate module path.\n");
    return;
  }
  module_path[path_size] = 0;

  dl = dlopen(module_path, RTLD_LAZY);
  if (!dl) {
    fprintf(stderr, "Failed to open module: %s\n", module_path);
    return;
  }

  funcptr = dlsym(dl, module_function);
  if (!funcptr) {
    fprintf(stderr, "Failed to find symbol: %s\n", module_function);
    return;
  }
  funcptr();

  dlclose(dl);
#endif
}

int main(int argc, char *argv[])
{
  fprintf(stdout, "Hello from program.c\n");
  fflush(stdout);

#if defined(PLATFORM_WIN)
  if (!GetModuleFileName(NULL, bin_path, MAX_PATH)) {
    fprintf(stderr, "Failed to determine executable path.\n");
    return 1;
  }
#elif defined(PLATFORM_MAC) || defined(PLATFORM_LINUX)
  // Using argv[0] should be OK here since we control how the tests run, and
  // can avoid exec and such issues that make it unreliable.
  if (!realpath(argv[0], bin_path)) {
    fprintf(stderr, "Failed to determine executable path (%s).\n", argv[0]);
    return 1;
  }
#endif

  CallModule("lib1");
  CallModule("lib2");
  return 0;
}
