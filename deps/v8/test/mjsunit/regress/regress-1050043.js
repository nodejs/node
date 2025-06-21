// Copyright 2008 the V8 project authors. All rights reserved.
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

function unsignedShiftRight(val, shift) {
  return val >>> shift;
}

assertEquals(        15, unsignedShiftRight(15, 0), "15 >>> 0");
assertEquals(         7, unsignedShiftRight(15, 1), "15 >>> 1");
assertEquals(         3, unsignedShiftRight(15, 2), "15 >>> 2");

assertEquals(4294967288, unsignedShiftRight(-8, 0), "-8 >>> 0");
assertEquals(2147483644, unsignedShiftRight(-8, 1), "-8 >>> 1");
assertEquals(1073741822, unsignedShiftRight(-8, 2), "-8 >>> 2");

assertEquals(         1, unsignedShiftRight(-8, 31), "-8 >>> 31");
assertEquals(4294967288, unsignedShiftRight(-8, 32), "-8 >>> 32");
assertEquals(2147483644, unsignedShiftRight(-8, 33), "-8 >>> 33");
assertEquals(1073741822, unsignedShiftRight(-8, 34), "-8 >>> 34");

assertEquals(2147483648, unsignedShiftRight(0x80000000, 0), "0x80000000 >>> 0");
assertEquals(1073741824, unsignedShiftRight(0x80000000, 1), "0x80000000 >>> 1");
assertEquals( 536870912, unsignedShiftRight(0x80000000, 2), "0x80000000 >>> 2");

assertEquals(1073741824, unsignedShiftRight(0x40000000, 0), "0x40000000 >>> 0");
assertEquals( 536870912, unsignedShiftRight(0x40000000, 1), "0x40000000 >>> 1");
assertEquals( 268435456, unsignedShiftRight(0x40000000, 2), "0x40000000 >>> 2");
