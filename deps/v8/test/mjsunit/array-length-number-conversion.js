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

// A reduced test case from Acid3 test 95.
// When an object is assigned to an array length field,
// it is converted to a number.

function CheckSetArrayLength(x, expected) {
  var a = [];
  a.length = x;

  assertEquals("number", typeof a.length);
  assertEquals(expected, a.length);
}

CheckSetArrayLength(2147483648, 2147483648);
CheckSetArrayLength("2147483648", 2147483648);
CheckSetArrayLength(null, 0);
CheckSetArrayLength(false, 0);
CheckSetArrayLength(true, 1);
CheckSetArrayLength({valueOf : function() { return 42; }}, 42);
CheckSetArrayLength({toString : function() { return '42'; }}, 42);

// Test invalid values
assertThrows("var y = []; y.length = 'abc';");
assertThrows("var y = []; y.length = undefined;");
assertThrows("var y = []; y.length = {};");
assertThrows("var y = []; y.length = -1;");
assertThrows("var y = []; y.length = {valueOf:function() { throw new Error(); }};");
