#ifndef NODE_PLATFORM_H_
#define NODE_PLATFORM_H_

#include <v8.h>

namespace node {

class Platform {
 public:
  static char** SetupArgs(int argc, char *argv[]);
  static void SetProcessTitle(char *title);
  static const char* GetProcessTitle(int *len);

  static int GetMemory(size_t *rss, size_t *vsize);
  static int GetExecutablePath(char* buffer, size_t* size);
  static int GetCPUInfo(v8::Local<v8::Array> *cpus);
  static double GetFreeMemory();
  static double GetTotalMemory();
  static double GetUptime();
  static int GetLoadAvg(v8::Local<v8::Array> *loads);
};


}  // namespace node
#endif  // NODE_PLATFORM_H_
