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

var except = "exception";

var correct_answer_index = 0;
var correct_answers = [
 false,  false,   true,   true,  false,  false,   true,   true,
  true,  false,  false,   true,   true,  false,  false,   true,
 false,   true,   true,  false,  false,   true,   true,  false,
  true,   true,  false,  false,   true,   true,  false,  false,
except, except,   true,   true, except, except,   true,   true,
except, except,  false,   true, except, except,  false,   true,
except, except,   true,  false, except, except,   true,  false,
except, except,  false,  false, except, except,  false,  false,
 false,  false, except, except,  false,  false, except, except,
  true,  false, except, except,   true,  false, except, except,
 false,   true, except, except,  false,   true, except, except,
  true,   true, except, except,   true,   true, except, except,
except, except, except, except, except, except, except, except,
except, except, except, except, except, except, except, except,
except, except, except, except, except, except, except, except,
except, except, except, except, except, except, except, except,
 false,  false,   true,   true,  false,  false,   true,   true,
  true,  false,  false,   true,  false,  false,   true,   true,
 false,   true,   true,  false,  false,   true,   true,  false,
  true,   true,  false,  false,  false,   true,   true,  false,
except, except,   true,   true, except, except,   true,   true,
except, except,  false,   true, except, except,   true,   true,
except, except,   true,  false, except, except,   true,  false,
except, except,  false,  false, except, except,   true,  false,
 false,  false, except, except,  false,  false, except, except,
  true,  false, except, except,   true,  false, except, except,
 false,   true, except, except,  false,   true, except, except,
  true,   true, except, except,   true,   true, except, except,
except, except, except, except, except, except, except, except,
except, except, except, except, except, except, except, except,
except, except, except, except, except, except, except, except,
except, except, except, except, except, except, except, except,
 false,  false,   true,   true,  false,   true,   true,  false,
  true,  false,  false,   true,   true,   true,  false,  false,
 false,   true,   true,  false,  false,   true,   true,  false,
  true,   true,  false,  false,   true,   true,  false,  false,
except, except,   true,   true, except, except,   true,   true,
except, except,  false,   true, except, except,  false,   true,
except, except,   true,  false, except, except,   true,  false,
except, except,  false,  false, except, except,  false,  false,
 false,  false, except, except,  false,   true, except, except,
  true,  false, except, except,   true,   true, except, except,
 false,   true, except, except,  false,   true, except, except,
  true,   true, except, except,   true,   true, except, except,
except, except, except, except, except, except, except, except,
except, except, except, except, except, except, except, except,
except, except, except, except, except, except, except, except,
except, except, except, except, except, except, except, except,
 false,  false,   true,   true,  false,   true,   true,  false,
  true,  false,  false,   true,  false,   true,   true,  false,
 false,   true,   true,  false,  false,   true,   true,  false,
  true,   true,  false,  false,  false,   true,   true,  false,
except, except,   true,   true, except, except,   true,   true,
except, except,  false,   true, except, except,   true,   true,
except, except,   true,  false, except, except,   true,  false,
except, except,  false,  false, except, except,   true,  false,
 false,  false, except, except,  false,   true, except, except,
  true,  false, except, except,   true,   true, except, except,
 false,   true, except, except,  false,   true, except, except,
  true,   true, except, except,   true,   true, except, except,
except, except, except, except, except, except, except, except,
except, except, except, except, except, except, except, except,
except, except, except, except, except, except, except, except,
except, except, except, except, except, except, except, except,
 false,  false,   true,   true,  false,  false,   true,   true,
  true,  false,  false,   true,  false,  false,   true,   true,
 false,   true,   true,  false,   true,   true,  false,  false,
  true,   true,  false,  false,   true,   true,  false,  false,
except, except,   true,   true, except, except,   true,   true,
except, except,  false,   true, except, except,   true,   true,
except, except,   true,  false, except, except,  false,  false,
except, except,  false,  false, except, except,  false,  false,
 false,  false, except, except,  false,  false, except, except,
  true,  false, except, except,  false,  false, except, except,
 false,   true, except, except,   true,   true, except, except,
  true,   true, except, except,   true,   true, except, except,
except, except, except, except, except, except, except, except,
except, except, except, except, except, except, except, except,
except, except, except, except, except, except, except, except,
except, except, except, except, except, except, except, except,
 false,  false,   true,   true,  false,  false,   true,   true,
  true,  false,  false,   true,  false,  false,   true,   true,
 false,   true,   true,  false,   true,   true,  false,  false,
  true,   true,  false,  false,   true,   true,  false,  false,
except, except,   true,   true, except, except,   true,   true,
except, except,  false,   true, except, except,   true,   true,
except, except,   true,  false, except, except,  false,  false,
except, except,  false,  false, except, except,  false,  false,
 false,  false, except, except,  false,  false, except, except,
  true,  false, except, except,  false,  false, except, except,
 false,   true, except, except,   true,   true, except, except,
  true,   true, except, except,   true,   true, except, except,
except, except, except, except, except, except, except, except,
except, except, except, except, except, except, except, except,
except, except, except, except, except, except, except, except,
except, except, except, except, except, except, except, except,
 false,  false,   true,   true,   true,   true,  false,  false,
  true,  false,  false,   true,   true,   true,  false,  false,
 false,   true,   true,  false,   true,   true,  false,  false,
  true,   true,  false,  false,   true,   true,  false,  false,
except, except,   true,   true, except, except,   true,   true,
except, except,  false,   true, except, except,   true,   true,
except, except,   true,  false, except, except,  false,  false,
except, except,  false,  false, except, except,  false,  false,
 false,  false, except, except,   true,   true, except, except,
  true,  false, except, except,   true,   true, except, except,
 false,   true, except, except,   true,   true, except, except,
  true,   true, except, except,   true,   true, except, except,
except, except, except, except, except, except, except, except,
except, except, except, except, except, except, except, except,
except, except, except, except, except, except, except, except,
except, except, except, except, except, except, except, except,
 false,  false,   true,   true,   true,   true,  false,  false,
  true,  false,  false,   true,   true,   true,  false,  false,
 false,   true,   true,  false,   true,   true,  false,  false,
  true,   true,  false,  false,   true,   true,  false,  false,
except, except,   true,   true, except, except,   true,   true,
except, except,  false,   true, except, except,   true,   true,
except, except,   true,  false, except, except,  false,  false,
except, except,  false,  false, except, except,  false,  false,
 false,  false, except, except,   true,   true, except, except,
  true,  false, except, except,   true,   true, except, except,
 false,   true, except, except,   true,   true, except, except,
  true,   true, except, except,   true,   true, except, except,
except, except, except, except, except, except, except, except,
except, except, except, except, except, except, except, except,
except, except, except, except, except, except, except, except,
except, except, except, except, except, except, except, except,
 false,  false,   true,   true,  false,  false,   true,   true,
  true,  false,  false,   true,   true,   true,  false,  false,
 false,   true,   true,  false,  false,  false,   true,   true,
  true,   true,  false,  false,   true,   true,  false,  false,
except, except,   true,   true, except, except,   true,   true,
except, except,  false,   true, except, except,  false,  false,
except, except,   true,  false, except, except,   true,   true,
except, except,  false,  false, except, except,  false,  false,
 false,  false, except, except,  false,  false, except, except,
  true,  false, except, except,   true,   true, except, except,
 false,   true, except, except,  false,  false, except, except,
  true,   true, except, except,   true,   true, except, except,
except, except, except, except, except, except, except, except,
except, except, except, except, except, except, except, except,
except, except, except, except, except, except, except, except,
except, except, except, except, except, except, except, except,
 false,  false,   true,   true,  false,  false,   true,   true,
  true,  false,  false,   true,  false,  false,   true,   true,
 false,   true,   true,  false,  false,  false,   true,   true,
  true,   true,  false,  false,  false,  false,   true,   true,
except, except,   true,   true, except, except,   true,   true,
except, except,  false,   true, except, except,   true,   true,
except, except,   true,  false, except, except,   true,   true,
except, except,  false,  false, except, except,   true,   true,
 false,  false, except, except,  false,  false, except, except,
  true,  false, except, except,   true,   true, except, except,
 false,   true, except, except,  false,  false, except, except,
  true,   true, except, except,   true,   true, except, except,
except, except, except, except, except, except, except, except,
except, except, except, except, except, except, except, except,
except, except, except, except, except, except, except, except,
except, except, except, except, except, except, except, except,
 false,  false,   true,   true,  false,  false,   true,   true,
  true,  false,  false,   true,   true,   true,  false,  false,
 false,   true,   true,  false,  false,  false,   true,   true,
  true,   true,  false,  false,   true,   true,  false,  false,
except, except,   true,   true, except, except,   true,   true,
except, except,  false,   true, except, except,  false,  false,
except, except,   true,  false, except, except,   true,   true,
except, except,  false,  false, except, except,  false,  false,
 false,  false, except, except,  false,  false, except, except,
  true,  false, except, except,   true,   true, except, except,
 false,   true, except, except,  false,  false, except, except,
  true,   true, except, except,   true,   true, except, except,
except, except, except, except, except, except, except, except,
except, except, except, except, except, except, except, except,
except, except, except, except, except, except, except, except,
except, except, except, except, except, except, except, except,
 false,  false,   true,   true,  false,  false,   true,   true,
  true,  false,  false,   true,  false,  false,   true,   true,
 false,   true,   true,  false,  false,  false,   true,   true,
  true,   true,  false,  false,  false,  false,   true,   true,
except, except,   true,   true, except, except,   true,   true,
except, except,  false,   true, except, except,   true,   true,
except, except,   true,  false, except, except,   true,   true,
except, except,  false,  false, except, except,   true,   true,
 false,  false, except, except,  false,  false, except, except,
  true,  false, except, except,   true,   true, except, except,
 false,   true, except, except,  false,  false, except, except,
  true,   true, except, except,   true,   true, except, except,
except, except, except, except, except, except, except, except,
except, except, except, except, except, except, except, except,
except, except, except, except, except, except, except, except,
except, except, except, except, except, except, except, except,
 false,  false,   true,   true,  false,  false,   true,   true,
  true,  false,  false,   true,  false,  false,   true,   true,
 false,   true,   true,  false,   true,   true,  false,  false,
  true,   true,  false,  false,   true,   true,  false,  false,
except, except,   true,   true, except, except,   true,   true,
except, except,  false,   true, except, except,   true,   true,
except, except,   true,  false, except, except,  false,  false,
except, except,  false,  false, except, except,  false,  false,
 false,  false, except, except,  false,  false, except, except,
  true,  false, except, except,  false,  false, except, except,
 false,   true, except, except,   true,   true, except, except,
  true,   true, except, except,   true,   true, except, except,
except, except, except, except, except, except, except, except,
except, except, except, except, except, except, except, except,
except, except, except, except, except, except, except, except,
except, except, except, except, except, except, except, except,
 false,  false,   true,   true,  false,  false,   true,   true,
  true,  false,  false,   true,  false,  false,   true,   true,
 false,   true,   true,  false,   true,   true,  false,  false,
  true,   true,  false,  false,   true,   true,  false,  false,
except, except,   true,   true, except, except,   true,   true,
except, except,  false,   true, except, except,   true,   true,
except, except,   true,  false, except, except,  false,  false,
except, except,  false,  false, except, except,  false,  false,
 false,  false, except, except,  false,  false, except, except,
  true,  false, except, except,  false,  false, except, except,
 false,   true, except, except,   true,   true, except, except,
  true,   true, except, except,   true,   true, except, except,
except, except, except, except, except, except, except, except,
except, except, except, except, except, except, except, except,
except, except, except, except, except, except, except, except,
except, except, except, except, except, except, except, except,
 false,  false,   true,   true,   true,   true,  false,  false,
  true,  false,  false,   true,   true,   true,  false,  false,
 false,   true,   true,  false,   true,   true,  false,  false,
  true,   true,  false,  false,   true,   true,  false,  false,
except, except,   true,   true, except, except,   true,   true,
except, except,  false,   true, except, except,   true,   true,
except, except,   true,  false, except, except,  false,  false,
except, except,  false,  false, except, except,  false,  false,
 false,  false, except, except,   true,   true, except, except,
  true,  false, except, except,   true,   true, except, except,
 false,   true, except, except,   true,   true, except, except,
  true,   true, except, except,   true,   true, except, except,
except, except, except, except, except, except, except, except,
except, except, except, except, except, except, except, except,
except, except, except, except, except, except, except, except,
except, except, except, except, except, except, except, except,
 false,  false,   true,   true,   true,   true,  false,  false,
  true,  false,  false,   true,   true,   true,  false,  false,
 false,   true,   true,  false,   true,   true,  false,  false,
  true,   true,  false,  false,   true,   true,  false,  false,
except, except,   true,   true, except, except,   true,   true,
except, except,  false,   true, except, except,   true,   true,
except, except,   true,  false, except, except,  false,  false,
except, except,  false,  false, except, except,  false,  false,
 false,  false, except, except,   true,   true, except, except,
  true,  false, except, except,   true,   true, except, except,
 false,   true, except, except,   true,   true, except, except,
  true,   true, except, except,   true,   true, except, except,
except, except, except, except, except, except, except, except,
except, except, except, except, except, except, except, except,
except, except, except, except, except, except, except, except,
except, except, except, except, except, except, except, except];

for (var i = 0; i < 256; i++) {
  Test(i & 1, i & 2, i & 4, i & 8, i & 0x10, i & 0x20, i & 0x40, i & 0x80);
}


function InstanceTest(x, func) {
  try {
    var answer = (x instanceof func);
    assertEquals(correct_answers[correct_answer_index], answer);
  } catch (e) {
    assertTrue(/prototype/.test(e));
    assertEquals(correct_answers[correct_answer_index], except);
  }
  correct_answer_index++;
}


function Test(a, b, c, d, e, f, g, h) {
  var Foo = function() { }
  var Bar = function() { }

  if (c) Foo.prototype = 12;
  if (d) Bar.prototype = 13;
  var x = a ? new Foo() : new Bar();
  var y = b ? new Foo() : new Bar();
  InstanceTest(x, Foo);
  InstanceTest(y, Foo);
  InstanceTest(x, Bar);
  InstanceTest(y, Bar);
  if (e) x.__proto__ = Bar.prototype;
  if (f) y.__proto__ = Foo.prototype;
  if (g) {
    x.__proto__ = y;
  } else {
    if (h) y.__proto__ = x
  }
  InstanceTest(x, Foo);
  InstanceTest(y, Foo);
  InstanceTest(x, Bar);
  InstanceTest(y, Bar);
}
