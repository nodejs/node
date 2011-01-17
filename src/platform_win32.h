#ifndef NODE_PLATFORM_WIN32_H_
#define NODE_PLATFORM_WIN32_H_

// Require at least Windows XP SP1
// (GetProcessId requires it)
#ifndef _WIN32_WINNT
# define _WIN32_WINNT 0x0501
#endif

#ifndef WIN32_LEAN_AND_MEAN
# define WIN32_LEAN_AND_MEAN // implies NOCRYPT and NOGDI.
#endif

#ifndef NOMINMAX
# define NOMINMAX
#endif

#ifndef NOKERNEL
# define NOKERNEL
#endif

#ifndef NOUSER
# define NOUSER
#endif

#ifndef NOSERVICE
# define NOSERVICE
#endif

#ifndef NOSOUND
# define NOSOUND
#endif

#ifndef NOMCX
# define NOMCX
#endif

#include <windows.h>

namespace node {

#define NO_IMPL_MSG(name...) \
    fprintf(stderr, "Not implemented: %s\n", #name);

const char *winapi_strerror(const int errorno);
void winapi_perror(const char* prefix);

}

#endif  // NODE_PLATFORM_WIN32_H_

