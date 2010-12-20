#include <node_os.h>

#include <node.h>
#include <v8.h>

#include <errno.h>

#ifdef __POSIX__
# include <unistd.h>  // gethostname
#else // __MINGW32__
# include <winsock2.h> // gethostname
#endif // __MINGW32__

namespace node {

using namespace v8;

static Handle<Value> GetHostname(const Arguments& args) {
  HandleScope scope;
  char s[255];
  int r = gethostname(s, 255);

  if (r < 0) {
#ifdef __MINGW32__
    errno = WSAGetLastError() - WSABASEERR;
#endif
    return ThrowException(ErrnoException(errno, "gethostname"));
  }

  return scope.Close(String::New(s));
}

void OS::Initialize(v8::Handle<v8::Object> target) {
  HandleScope scope;

  NODE_SET_METHOD(target, "getHostname", GetHostname);
}


}  // namespace node

NODE_MODULE(node_os, node::OS::Initialize);
