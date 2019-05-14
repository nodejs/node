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

// See: http://code.google.com/p/v8/issues/detail?id=1015

// Object and array literals should be created using DefineOwnProperty, and
// therefore not hit setters in the prototype.

function mkFail(message) {
  return function () { assertUnreachable(message); }
}

Object.defineProperty(Object.prototype, "foo",
                      {get: mkFail("oget"), set: mkFail("oset")});
Object.defineProperty(Array.prototype, "2",
                      {get: mkFail("aget"), set: mkFail("aset")});

function inFunction() {
  for (var i = 0; i < 10; i++) {
    // in loop.
    var ja = JSON.parse('[1,2,3,4]');
    var jo = JSON.parse('{"bar": 10, "foo": 20}')
    var jop = JSON.parse('{"bar": 10, "__proto__": { }, "foo": 20}')
    var a = [1,2,3,4];
    var o = { bar: 10, foo: 20 };
    var op = { __proto__: { set bar(v) { assertUnreachable("bset"); } },
               bar: 10 };
  }
}

for (var i = 0; i < 10; i++) {
  // In global scope.
  var ja = JSON.parse('[1,2,3,4]');
  var jo = JSON.parse('{"bar": 10, "foo": 20}')
  var jop = JSON.parse('{"bar": 10, "__proto__": { }, "foo": 20}')
  var a = [1,2,3,4];
  var o = { bar: 10, foo: 20 };
  var op = { __proto__: { set bar(v) { assertUnreachable("bset"); } },
             bar: 10 };
  // In function scope.
  inFunction();
}
