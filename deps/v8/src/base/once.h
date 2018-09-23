// Copyright 2012 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// emulates google3/base/once.h
//
// This header is intended to be included only by v8's internal code. Users
// should not use this directly.
//
// This is basically a portable version of pthread_once().
//
// This header declares:
// * A type called OnceType.
// * A macro V8_DECLARE_ONCE() which declares a (global) variable of type
//   OnceType.
// * A function CallOnce(OnceType* once, void (*init_func)()).
//   This function, when invoked multiple times given the same OnceType object,
//   will invoke init_func on the first call only, and will make sure none of
//   the calls return before that first call to init_func has finished.
//
// Additionally, the following features are supported:
// * A macro V8_ONCE_INIT which is expanded into the expression used to
//   initialize a OnceType. This is only useful when clients embed a OnceType
//   into a structure of their own and want to initialize it statically.
// * The user can provide a parameter which CallOnce() forwards to the
//   user-provided function when it is called. Usage example:
//     CallOnce(&my_once, &MyFunctionExpectingIntArgument, 10);
// * This implementation guarantees that OnceType is a POD (i.e. no static
//   initializer generated).
//
// This implements a way to perform lazy initialization.  It's more efficient
// than using mutexes as no lock is needed if initialization has already
// happened.
//
// Example usage:
//   void Init();
//   V8_DECLARE_ONCE(once_init);
//
//   // Calls Init() exactly once.
//   void InitOnce() {
//     CallOnce(&once_init, &Init);
//   }
//
// Note that if CallOnce() is called before main() has begun, it must
// only be called by the thread that will eventually call main() -- that is,
// the thread that performs dynamic initialization.  In general this is a safe
// assumption since people don't usually construct threads before main() starts,
// but it is technically not guaranteed.  Unfortunately, Win32 provides no way
// whatsoever to statically-initialize its synchronization primitives, so our
// only choice is to assume that dynamic initialization is single-threaded.

#ifndef V8_BASE_ONCE_H_
#define V8_BASE_ONCE_H_

#include <stddef.h>

#include "src/base/atomicops.h"

namespace v8 {
namespace base {

typedef AtomicWord OnceType;

#define V8_ONCE_INIT 0

#define V8_DECLARE_ONCE(NAME) ::v8::base::OnceType NAME

enum {
  ONCE_STATE_UNINITIALIZED = 0,
  ONCE_STATE_EXECUTING_FUNCTION = 1,
  ONCE_STATE_DONE = 2
};

typedef void (*NoArgFunction)();
typedef void (*PointerArgFunction)(void* arg);

template <typename T>
struct OneArgFunction {
  typedef void (*type)(T);
};

void CallOnceImpl(OnceType* once, PointerArgFunction init_func, void* arg);

inline void CallOnce(OnceType* once, NoArgFunction init_func) {
  if (Acquire_Load(once) != ONCE_STATE_DONE) {
    CallOnceImpl(once, reinterpret_cast<PointerArgFunction>(init_func), NULL);
  }
}


template <typename Arg>
inline void CallOnce(OnceType* once,
    typename OneArgFunction<Arg*>::type init_func, Arg* arg) {
  if (Acquire_Load(once) != ONCE_STATE_DONE) {
    CallOnceImpl(once, reinterpret_cast<PointerArgFunction>(init_func),
        static_cast<void*>(arg));
  }
}

} }  // namespace v8::base

#endif  // V8_BASE_ONCE_H_
