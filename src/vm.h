#pragma once

#ifdef _MSC_VER
#pragma warning(push)
#pragma warning(disable : 4996)  // deprecated declaration
#endif  // _MSC_VER
#ifdef __GNUC__
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wdeprecated-declarations"
#endif  // __GNUC__
#include "v8.h"
#ifdef __GNUC__
#pragma GCC diagnostic pop
#endif  // __GNUC__
#ifdef _MSC_VER
#pragma warning(pop)
#endif  // _MSC_VER
