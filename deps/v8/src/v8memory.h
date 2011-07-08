// Copyright 2006-2008 the V8 project authors. All rights reserved.
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

#ifndef V8_MEMORY_H_
#define V8_MEMORY_H_

namespace v8 {
namespace internal {

// Memory provides an interface to 'raw' memory. It encapsulates the casts
// that typically are needed when incompatible pointer types are used.

class Memory {
 public:
  static uint8_t& uint8_at(Address addr) {
    return *reinterpret_cast<uint8_t*>(addr);
  }

  static uint16_t& uint16_at(Address addr)  {
    return *reinterpret_cast<uint16_t*>(addr);
  }

  static uint32_t& uint32_at(Address addr)  {
    return *reinterpret_cast<uint32_t*>(addr);
  }

  static int32_t& int32_at(Address addr)  {
    return *reinterpret_cast<int32_t*>(addr);
  }

  static uint64_t& uint64_at(Address addr)  {
    return *reinterpret_cast<uint64_t*>(addr);
  }

  static int& int_at(Address addr)  {
    return *reinterpret_cast<int*>(addr);
  }

  static double& double_at(Address addr)  {
    return *reinterpret_cast<double*>(addr);
  }

  static Address& Address_at(Address addr)  {
    return *reinterpret_cast<Address*>(addr);
  }

  static Object*& Object_at(Address addr)  {
    return *reinterpret_cast<Object**>(addr);
  }

  static Handle<Object>& Object_Handle_at(Address addr)  {
    return *reinterpret_cast<Handle<Object>*>(addr);
  }
};

} }  // namespace v8::internal

#endif  // V8_MEMORY_H_
