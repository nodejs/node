// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef Collections_h
#define Collections_h

#if V8_INSPECTOR_USE_STL
#include "platform/inspector_protocol/CollectionsSTL.h"
#else
#include "platform/inspector_protocol/CollectionsWTF.h"
#endif // V8_INSPECTOR_USE_STL


// Macro that returns a compile time constant with the length of an array, but gives an error if passed a non-array.
template<typename T, size_t Size> char (&ArrayLengthHelperFunction(T (&)[Size]))[Size];
// GCC needs some help to deduce a 0 length array.
#if defined(__GNUC__)
template<typename T> char (&ArrayLengthHelperFunction(T (&)[0]))[0];
#endif
#define PROTOCOL_ARRAY_LENGTH(array) sizeof(::ArrayLengthHelperFunction(array))

#endif // !defined(Collections_h)
