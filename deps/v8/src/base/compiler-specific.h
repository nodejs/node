// Copyright 2014 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_BASE_COMPILER_SPECIFIC_H_
#define V8_BASE_COMPILER_SPECIFIC_H_

#include "include/v8config.h"

// Annotate a typedef or function indicating it's ok if it's not used.
// Use like:
//   typedef Foo Bar ALLOW_UNUSED_TYPE;
#if V8_HAS_ATTRIBUTE_UNUSED
#define ALLOW_UNUSED_TYPE __attribute__((unused))
#else
#define ALLOW_UNUSED_TYPE
#endif


// Annotate a virtual method indicating it must be overriding a virtual
// method in the parent class.
// Use like:
//   virtual void bar() OVERRIDE;
#if V8_HAS_CXX11_OVERRIDE
#define OVERRIDE override
#else
#define OVERRIDE /* NOT SUPPORTED */
#endif


// Annotate a virtual method indicating that subclasses must not override it,
// or annotate a class to indicate that it cannot be subclassed.
// Use like:
//   class B FINAL : public A {};
//   virtual void bar() FINAL;
#if V8_HAS_CXX11_FINAL
#define FINAL final
#elif V8_HAS___FINAL
#define FINAL __final
#else
#define FINAL /* NOT SUPPORTED */
#endif


// Annotate a function indicating the caller must examine the return value.
// Use like:
//   int foo() WARN_UNUSED_RESULT;
#if V8_HAS_ATTRIBUTE_WARN_UNUSED_RESULT
#define WARN_UNUSED_RESULT __attribute__((warn_unused_result))
#else
#define WARN_UNUSED_RESULT /* NOT SUPPORTED */
#endif

#endif  // V8_BASE_COMPILER_SPECIFIC_H_
