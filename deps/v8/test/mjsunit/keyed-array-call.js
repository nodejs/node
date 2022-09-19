// Copyright 2013 the V8 project authors. All rights reserved.
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

var a = [function(a) { return a+10; },
         function(a) { return a+20; }];
a.__proto__.test = function(a) { return a+30; }
function f(i) {
  return "r" + (1, a[i](i+1), a[i](i+2));
}

assertEquals("r12", f(0));
assertEquals("r12", f(0));
assertEquals("r23", f(1));
assertEquals("r23", f(1));

// Deopt the stub.
assertEquals("rtest230", f("test"));

var a2 = [function(a) { return a+10; },,
          function(a) { return a+20; }];
a2.__proto__.test = function(a) { return a+30; }
function f2(i) {
  return "r" + (1, a2[i](i+1), a2[i](i+2));
}

assertEquals("r12", f2(0));
assertEquals("r12", f2(0));
assertEquals("r24", f2(2));
assertEquals("r24", f2(2));

// Deopt the stub. This will throw given that undefined is not a function.
assertThrows(function() { f2(1) });
