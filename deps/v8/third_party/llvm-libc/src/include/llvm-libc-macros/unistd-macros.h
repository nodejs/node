#ifndef LLVM_LIBC_MACROS_UNISTD_MACROS_H
#define LLVM_LIBC_MACROS_UNISTD_MACROS_H

#ifdef __linux__
#include "linux/unistd-macros.h"
#endif

#define STDIN_FILENO 0
#define STDOUT_FILENO 1
#define STDERR_FILENO 2

#endif // LLVM_LIBC_MACROS_UNISTD_MACROS_H
