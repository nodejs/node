// Copyright 2010 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_GDB_JIT_H_
#define V8_GDB_JIT_H_

#include "include/v8.h"

//
// GDB has two ways of interacting with JIT code.  With the "JIT compilation
// interface", V8 can tell GDB when it emits JIT code.  Unfortunately to do so,
// it has to create platform-native object files, possibly with platform-native
// debugging information.  Currently only ELF and Mach-O are supported, which
// limits this interface to Linux and Mac OS.  This JIT compilation interface
// was introduced in GDB 7.0.  V8 support can be enabled with the --gdbjit flag.
//
// The other way that GDB can know about V8 code is via the "custom JIT reader"
// interface, in which a GDB extension parses V8's private data to determine the
// function, file, and line of a JIT frame, and how to unwind those frames.
// This interface was introduced in GDB 7.6.  This interface still relies on V8
// to register its code via the JIT compilation interface, but doesn't require
// that V8 create ELF images.  Support will be added for this interface in the
// future.
//

namespace v8 {
namespace internal {
namespace GDBJITInterface {
#ifdef ENABLE_GDB_JIT_INTERFACE
// JitCodeEventHandler that creates ELF/Mach-O objects and registers them with
// GDB.
void EventHandler(const v8::JitCodeEvent* event);
#endif
}  // namespace GDBJITInterface
}  // namespace internal
}  // namespace v8

#endif
