// Copyright 2016 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_BASE_BASE_EXPORT_H_
#define V8_BASE_BASE_EXPORT_H_

#include "include/v8config.h"

#if V8_OS_WIN

#ifdef BUILDING_V8_BASE_SHARED
#define V8_BASE_EXPORT __declspec(dllexport)
#elif USING_V8_BASE_SHARED
#define V8_BASE_EXPORT __declspec(dllimport)
#else
#define V8_BASE_EXPORT
#endif  // BUILDING_V8_BASE_SHARED

#else  // !V8_OS_WIN

// Setup for Linux shared library export.
#ifdef BUILDING_V8_BASE_SHARED
#define V8_BASE_EXPORT __attribute__((visibility("default")))
#else
#define V8_BASE_EXPORT
#endif

#endif  // V8_OS_WIN

#endif  // V8_BASE_BASE_EXPORT_H_
