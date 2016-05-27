// Copyright 2014 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_LIBPLATFORM_LIBPLATFORM_H_
#define V8_LIBPLATFORM_LIBPLATFORM_H_

#include "v8-platform.h"  // NOLINT(build/include)

namespace v8 {
namespace platform {

/**
 * Returns a new instance of the default v8::Platform implementation.
 *
 * The caller will take ownership of the returned pointer. |thread_pool_size|
 * is the number of worker threads to allocate for background jobs. If a value
 * of zero is passed, a suitable default based on the current number of
 * processors online will be chosen.
 */
v8::Platform* CreateDefaultPlatform(int thread_pool_size = 0);


/**
 * Pumps the message loop for the given isolate.
 *
 * The caller has to make sure that this is called from the right thread.
 * Returns true if a task was executed, and false otherwise. This call does
 * not block if no task is pending. The |platform| has to be created using
 * |CreateDefaultPlatform|.
 */
bool PumpMessageLoop(v8::Platform* platform, v8::Isolate* isolate);


}  // namespace platform
}  // namespace v8

#endif  // V8_LIBPLATFORM_LIBPLATFORM_H_
