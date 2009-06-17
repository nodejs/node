// Copyright 2009 the V8 project authors. All rights reserved.
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are
// met:
//
//     * Redistributions of source code must retain the above copyright
//       notice, this list of conditions and the following disclaimer.
//     * Redistributions in binary form must reproduce the above
//       copyright notice, this list of conditions and the following
//       disclaimer in the documentation and/or other materials provided
//       with the distribution.
//     * Neither the name of Google Inc. nor the names of its
//       contributors may be used to endorse or promote products derived
//       from this software without specific prior written permission.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
// "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
// LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
// A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
// OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
// SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
// LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
// DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
// THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
// (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
// OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

#ifndef V8_X64_SIMULATOR_X64_H_
#define V8_X64_SIMULATOR_X64_H_


// Since there is no simulator for the ia32 architecture the only thing we can
// do is to call the entry directly.
// TODO(X64): Don't pass p0, since it isn't used?
#define CALL_GENERATED_CODE(entry, p0, p1, p2, p3, p4) \
  entry(p0, p1, p2, p3, p4);

// Calculated the stack limit beyond which we will throw stack overflow errors.
// This macro must be called from a C++ method. It relies on being able to take
// the address of "this" to get a value on the current execution stack and then
// calculates the stack limit based on that value.
// NOTE: The check for overflow is not safe as there is no guarantee that the
// running thread has its stack in all memory up to address 0x00000000.
#define GENERATED_CODE_STACK_LIMIT(limit) \
  (reinterpret_cast<uintptr_t>(this) >= limit ? \
      reinterpret_cast<uintptr_t>(this) - limit : 0)

#endif  // V8_X64_SIMULATOR_X64_H_
