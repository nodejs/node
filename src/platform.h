// Copyright Joyent, Inc. and other Node contributors.
//
// Permission is hereby granted, free of charge, to any person obtaining a
// copy of this software and associated documentation files (the
// "Software"), to deal in the Software without restriction, including
// without limitation the rights to use, copy, modify, merge, publish,
// distribute, sublicense, and/or sell copies of the Software, and to permit
// persons to whom the Software is furnished to do so, subject to the
// following conditions:
//
// The above copyright notice and this permission notice shall be included
// in all copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
// OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
// MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN
// NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM,
// DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR
// OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE
// USE OR OTHER DEALINGS IN THE SOFTWARE.

#ifndef NODE_PLATFORM_H_
#define NODE_PLATFORM_H_

#include <v8.h>

namespace node {

class Platform {
 public:
  static char** SetupArgs(int argc, char *argv[]);
  static void SetProcessTitle(char *title);
  static const char* GetProcessTitle(int *len);

  static int GetMemory(size_t *rss);
  static int GetCPUInfo(v8::Local<v8::Array> *cpus);
  static double GetUptime(bool adjusted = false)
  {
    return adjusted ? GetUptimeImpl() - prog_start_time : GetUptimeImpl();
  }
  static v8::Handle<v8::Value> GetInterfaceAddresses();
 private:
  static double GetUptimeImpl();
  static double prog_start_time;
};


}  // namespace node
#endif  // NODE_PLATFORM_H_
