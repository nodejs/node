// Copyright 2022 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_COMMON_ALLOW_DEPRECATED_H_
#define V8_COMMON_ALLOW_DEPRECATED_H_

#if defined(V8_IMMINENT_DEPRECATION_WARNINGS) || \
    defined(V8_DEPRECATION_WARNINGS)

#if defined(V8_CC_MSVC)

#define START_ALLOW_USE_DEPRECATED() \
  __pragma(warning(push)) __pragma(warning(disable : 4996))

#define END_ALLOW_USE_DEPRECATED() __pragma(warning(pop))

#else  // !defined(V8_CC_MSVC)

#define START_ALLOW_USE_DEPRECATED() \
  _Pragma("GCC diagnostic push")     \
      _Pragma("GCC diagnostic ignored \"-Wdeprecated-declarations\"")

#define END_ALLOW_USE_DEPRECATED() _Pragma("GCC diagnostic pop")

#endif  // !defined(V8_CC_MSVC)

#else  // !(defined(V8_IMMINENT_DEPRECATION_WARNINGS) ||
       // defined(V8_DEPRECATION_WARNINGS))

#define START_ALLOW_USE_DEPRECATED()
#define END_ALLOW_USE_DEPRECATED()

#endif  // !(defined(V8_IMMINENT_DEPRECATION_WARNINGS) ||
        // defined(V8_DEPRECATION_WARNINGS))

#endif  // V8_COMMON_ALLOW_DEPRECATED_H_
