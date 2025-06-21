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

// In the assertions but the first, the ES5 spec actually requires 0, but
// that is arguably a spec bug, and other browsers return 1 like us.
// In ES6, all of those will presumably result in a ReferenceError.
// Our main concern with this test is that we do not crash, though.

function f1() {
  var XXX = 0
  try { throw 1 } catch (XXX) {
    eval("var h = function() { return XXX }")
  }
  return h()
}
assertEquals(1, f1())

function f2() {
  var XXX = 0
  try { throw 1 } catch (XXX) {
    eval("function h(){ return XXX }")
  }
  return h()
}
assertEquals(1, f2())

function f3() {
  var XXX = 0
  try { throw 1 } catch (XXX) {
    try { throw 2 } catch (y) {
      eval("function h(){ return XXX }")
    }
  }
  return h()
}
assertEquals(1, f3())

function f4() {
  var XXX = 0
  try { throw 1 } catch (XXX) {
    with ({}) {
      eval("function h(){ return XXX }")
    }
  }
  return h()
}
assertEquals(1, f4())

function f5() {
  var XXX = 0
  try { throw 1 } catch (XXX) {
    eval('eval("function h(){ return XXX }")')
  }
  return h()
}
assertEquals(1, f5())

function f6() {
  var XXX = 0
  try { throw 1 } catch (XXX) {
    eval("var h = (function() { function g(){ return XXX } return g })()")
  }
  return h()
}
assertEquals(1, f6())

function f7() {
  var XXX = 0
  try { throw 1 } catch (XXX) {
    eval("function h() { var XXX=2; function g(){ return XXX } return g }")
  }
  return h()()
}
assertEquals(2, f7())  // !

var XXX = 0
try { throw 1 } catch (XXX) {
  eval("function h(){ return XXX }")
}
assertEquals(1, h())
