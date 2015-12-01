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

// Flags: --allow-natives-syntax

// Should not take a very long time (n^2 algorithms are bad)

function do_slices() {
  var data = new Array(1024 * 12); // 12kB

  for (var i = 0; i < data.length; i++) {
    data[i] = 255;
  }

  var start = Date.now();

  for (i = 0; i < 20000; i++) {
    data.slice(4, 1);
  }

  return Date.now() - start;
}

// Should never take more than 3 seconds (if the bug is fixed, the test takes
// considerably less time than 3 seconds).
assertTrue(do_slices() < (3 * 1000));

// Make sure that packed and unpacked array slices are still properly handled
var holey_array = [1, 2, 3, 4, 5,,,,,,];
assertEquals([undefined], holey_array.slice(6, 7));
assertEquals(undefined, holey_array.slice(6, 7)[0]);
assertEquals([], holey_array.slice(2, 1));
assertEquals(3, holey_array.slice(2, 3)[0]);
