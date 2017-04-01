#ifndef SRC_PRINT_H_
#define SRC_PRINT_H_

#include <stdio.h>

// A minimal subset of print functions which are actually
// used in the code. To let the embedders to redefine them.

#define PrintF printf
#define FPrintF fprintf
#define VFPrintF vfprintf
#define FFlush fflush

#endif  // SRC_PRINT_H_