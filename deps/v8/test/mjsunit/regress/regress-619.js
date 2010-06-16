// Copyright 2010 the V8 project authors. All rights reserved.
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

// Tests that Object.defineProperty works correctly on array indices. 
// Please see http://code.google.com/p/v8/issues/detail?id=619 for details.

var obj = {};
obj[1] = 42;
assertEquals(42, obj[1]);
Object.defineProperty(obj, '1', {value:10, writable:false});
assertEquals(10, obj[1]);

// We should not be able to override obj[1].
obj[1] = 5;
assertEquals(10, obj[1]);

// Try on a range of numbers.
for(var i = 0; i < 1024; i++) {
  obj[i] = 42;
}

for(var i = 0; i < 1024; i++) {
  Object.defineProperty(obj, i, {value: i, writable:false});
}

for(var i = 0; i < 1024; i++) {
  assertEquals(i, obj[i]);
}

for(var i = 0; i < 1024; i++) {
  obj[1] = 5;
}

for(var i = 0; i < 1024; i++) {
  assertEquals(i, obj[i]);
}

