// Copyright 2007-2010 The JsonCpp Authors
// Distributed under MIT license, or public domain if desired and
// recognized in your jurisdiction.
// See file LICENSE for detail or copy at http://jsoncpp.sourceforge.net/LICENSE

#ifndef FUZZ_H_INCLUDED
#define FUZZ_H_INCLUDED

#include <cstddef>
#include <stdint.h>

extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size);

#endif // ifndef FUZZ_H_INCLUDED
