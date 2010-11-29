#ifndef NODE_PLATFORM_WIN32_H_
#define NODE_PLATFORM_WIN32_H_

#include <windows.h>

#define NO_IMPL(type, name, rv, args...)                                    \
          type  name ( args ) {                                             \
            HandleScope scope;                                              \
            fprintf(stderr, "Not implemented: "#type" "#name"("#args")\n"); \
            return rv;                                                      \
          }

#define RET_V8INT(value) \
          scope.Close(Integer::New(value));
#define RET_V8UNDEFINED \
          Undefined()
#define RET_V8TRUE \
          True()
#define RET_V8FALSE \
          False()

#define NO_IMPL_MSG(name...) \
          fprintf(stderr, "Not implemented: %s\n", #name);

namespace node {

void winapi_perror(const char* prefix);

}

#endif  // NODE_PLATFORM_WIN32_H_