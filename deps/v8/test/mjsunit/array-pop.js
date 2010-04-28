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

// Check pops with various number of arguments.
(function() {
  var a = [];
  for (var i = 0; i < 7; i++) {
    a = [7, 6, 5, 4, 3, 2, 1];

    assertEquals(1, a.pop(), "1st pop");
    assertEquals(6, a.length, "length 1st pop");

    assertEquals(2, a.pop(1), "2nd pop");
    assertEquals(5, a.length, "length 2nd pop");

    assertEquals(3, a.pop(1, 2), "3rd pop");
    assertEquals(4, a.length, "length 3rd pop");

    assertEquals(4, a.pop(1, 2, 3), "4th pop");
    assertEquals(3, a.length, "length 4th pop");

    assertEquals(5, a.pop(), "5th pop");
    assertEquals(2, a.length, "length 5th pop");

    assertEquals(6, a.pop(), "6th pop");
    assertEquals(1, a.length, "length 6th pop");

    assertEquals(7, a.pop(), "7th pop");
    assertEquals(0, a.length, "length 7th pop");

    assertEquals(undefined, a.pop(), "8th pop");
    assertEquals(0, a.length, "length 8th pop");

    assertEquals(undefined, a.pop(1, 2, 3), "9th pop");
    assertEquals(0, a.length, "length 9th pop");
  }
})();

// Test the case of not JSArray receiver.
// Regression test for custom call generators, see issue 684.
(function() {
  var a = [];
  for (var i = 0; i < 100; i++) a.push(i);
  var x = {__proto__: a};
  for (var i = 0; i < 100; i++) {
    assertEquals(99 - i, x.pop(), i + 'th iteration');
  }
})();
