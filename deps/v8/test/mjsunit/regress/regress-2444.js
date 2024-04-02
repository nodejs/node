// Copyright 2012 the V8 project authors. All rights reserved.
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


var flags;

function resetFlags(size) {
  flags = Array(size);
  while (size--) flags[size] = 0;
}

function assertFlags(array) {
  assertArrayEquals(array, flags);
}

function object_factory(flag_index, value, expected_flags) {
  var obj = {};
  obj.valueOf = function() {
    assertFlags(expected_flags);
    flags[flag_index]++;
    return value;
  }
  return obj;
}


assertEquals(-Infinity, Math.max());

resetFlags(1);
assertEquals(NaN,
             Math.max(object_factory(0, NaN, [0])));
assertFlags([1]);

resetFlags(2);
assertEquals(NaN,
             Math.max(object_factory(0, NaN, [0, 0]),
                      object_factory(1,   0, [1, 0])));
assertFlags([1, 1]);

resetFlags(3);
assertEquals(NaN,
             Math.max(object_factory(0, NaN, [0, 0, 0]),
                      object_factory(1,   0, [1, 0, 0]),
                      object_factory(2,   1, [1, 1, 0])));
assertFlags([1, 1, 1]);

resetFlags(3);
assertEquals(NaN,
             Math.max(object_factory(0,   2, [0, 0, 0]),
                      object_factory(1,   0, [1, 0, 0]),
                      object_factory(2, NaN, [1, 1, 0])));
assertFlags([1, 1, 1]);

resetFlags(3);
assertEquals(2,
             Math.max(object_factory(0,   2, [0, 0, 0]),
                      object_factory(1,   0, [1, 0, 0]),
                      object_factory(2,   1, [1, 1, 0])));
assertFlags([1, 1, 1]);


assertEquals(+Infinity, Math.min());

resetFlags(1);
assertEquals(NaN,
             Math.min(object_factory(0, NaN, [0])));
assertFlags([1]);

resetFlags(2);
assertEquals(NaN,
             Math.min(object_factory(0, NaN, [0, 0]),
                      object_factory(1,   0, [1, 0])));
assertFlags([1, 1]);

resetFlags(3);
assertEquals(NaN,
             Math.min(object_factory(0, NaN, [0, 0, 0]),
                      object_factory(1,   0, [1, 0, 0]),
                      object_factory(2,   1, [1, 1, 0])));
assertFlags([1, 1, 1]);

resetFlags(3);
assertEquals(NaN,
             Math.min(object_factory(0,   2, [0, 0, 0]),
                      object_factory(1,   0, [1, 0, 0]),
                      object_factory(2, NaN, [1, 1, 0])));
assertFlags([1, 1, 1]);

resetFlags(3);
assertEquals(0,
             Math.min(object_factory(0,   2, [0, 0, 0]),
                      object_factory(1,   0, [1, 0, 0]),
                      object_factory(2,   1, [1, 1, 0])));
assertFlags([1, 1, 1]);
