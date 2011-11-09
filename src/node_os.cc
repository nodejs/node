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


#include <node.h>
#include <node_os.h>
#include "platform.h"

#include <v8.h>

#include <errno.h>
#include <string.h>

#ifdef __MINGW32__
# include <io.h>

# include <platform_win32.h>
#endif

#ifdef __POSIX__
# include <unistd.h>  // gethostname, sysconf
# include <sys/utsname.h>
#endif

namespace node {

using namespace v8;

static Handle<Value> GetHostname(const Arguments& args) {
  HandleScope scope;
  char s[255];
  int r = gethostname(s, 255);

  if (r < 0) {
#ifdef __POSIX__
    return ThrowException(ErrnoException(errno, "gethostname"));
#else // __MINGW32__
    return ThrowException(ErrnoException(WSAGetLastError(), "gethostname"));
#endif // __MINGW32__
  }

  return scope.Close(String::New(s));
}

static Handle<Value> GetOSType(const Arguments& args) {
  HandleScope scope;

#ifdef __POSIX__
  char type[256];
  struct utsname info;

  uname(&info);
  strncpy(type, info.sysname, strlen(info.sysname));
  type[strlen(info.sysname)] = 0;

  return scope.Close(String::New(type));
#else // __MINGW32__
  return scope.Close(String::New("Windows_NT"));
#endif
}

static Handle<Value> GetOSRelease(const Arguments& args) {
  HandleScope scope;
  char release[256];

#ifdef __POSIX__
  struct utsname info;

  uname(&info);
  strncpy(release, info.release, strlen(info.release));
  release[strlen(info.release)] = 0;

#else // __MINGW32__
  OSVERSIONINFO info;
  info.dwOSVersionInfoSize = sizeof(info);

  if (GetVersionEx(&info) == 0) {
    return Undefined();
  }

  sprintf(release, "%d.%d.%d", static_cast<int>(info.dwMajorVersion),
      static_cast<int>(info.dwMinorVersion), static_cast<int>(info.dwBuildNumber));
#endif

  return scope.Close(String::New(release));
}

static Handle<Value> GetCPUInfo(const Arguments& args) {
  HandleScope scope;
  Local<Array> cpus;
  int r = Platform::GetCPUInfo(&cpus);

  if (r < 0) {
    return Undefined();
  }

  return scope.Close(cpus);
}

static Handle<Value> GetFreeMemory(const Arguments& args) {
  HandleScope scope;
  double amount = uv_get_free_memory();

  if (amount < 0) {
    return Undefined();
  }

  return scope.Close(Number::New(amount));
}

static Handle<Value> GetTotalMemory(const Arguments& args) {
  HandleScope scope;
  double amount = uv_get_total_memory();

  if (amount < 0) {
    return Undefined();
  }

  return scope.Close(Number::New(amount));
}

static Handle<Value> GetUptime(const Arguments& args) {
  HandleScope scope;
  double uptime = Platform::GetUptime();

  if (uptime < 0) {
    return Undefined();
  }

  return scope.Close(Number::New(uptime));
}

static Handle<Value> GetLoadAvg(const Arguments& args) {
  HandleScope scope;
  double loadavg[3];
  uv_loadavg(loadavg);

  Local<Array> loads = Array::New(3);
  loads->Set(0, Number::New(loadavg[0]));
  loads->Set(1, Number::New(loadavg[1]));
  loads->Set(2, Number::New(loadavg[2]));

  return scope.Close(loads);
}


static Handle<Value> GetInterfaceAddresses(const Arguments& args) {
  return Platform::GetInterfaceAddresses();
}


void OS::Initialize(v8::Handle<v8::Object> target) {
  HandleScope scope;

  NODE_SET_METHOD(target, "getHostname", GetHostname);
  NODE_SET_METHOD(target, "getLoadAvg", GetLoadAvg);
  NODE_SET_METHOD(target, "getUptime", GetUptime);
  NODE_SET_METHOD(target, "getTotalMem", GetTotalMemory);
  NODE_SET_METHOD(target, "getFreeMem", GetFreeMemory);
  NODE_SET_METHOD(target, "getCPUs", GetCPUInfo);
  NODE_SET_METHOD(target, "getOSType", GetOSType);
  NODE_SET_METHOD(target, "getOSRelease", GetOSRelease);
  NODE_SET_METHOD(target, "getInterfaceAddresses", GetInterfaceAddresses);
}


}  // namespace node

NODE_MODULE(node_os, node::OS::Initialize)
