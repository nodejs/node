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

// Flags: --max-new-space-size=256

// Check for GC bug constructing exceptions.
var v = [1, 2, 3, 4]

Object.preventExtensions(v);

function foo() {
  var re = /2147483647/;  // Equal to 0x7fffffff.
  for  (var i = 0; i < 10000; i++) {
    var ok = false;
    try {
      var j = 1;
      // Allocate some heap numbers in order to randomize the behaviour of the
      // garbage collector.  93 is chosen to be a prime number to avoid the
      // allocation settling into a too neat pattern.
      for (var j = 0; j < i % 93; j++) {
        j *= 1.123567;  // An arbitrary floating point number.
      }
      v[0x7fffffff] = 0;  // Trigger exception.
      assertTrue(false);
      return j;  // Make sure that future optimizations don't eliminate j.
    } catch(e) {
      ok = true;
      assertTrue(re.test(e));
    }
    assertTrue(ok);
  }
}

foo();
