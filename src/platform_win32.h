#ifndef NODE_PLATFORM_WIN32_H_
#define NODE_PLATFORM_WIN32_H_

#include <windows.h>



namespace node {

#define NO_IMPL_MSG(name...) \
    fprintf(stderr, "Not implemented: %s\n", #name);

void winapi_perror(const char* prefix);

}

#endif  // NODE_PLATFORM_WIN32_H_
