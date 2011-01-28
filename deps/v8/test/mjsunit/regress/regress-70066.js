// Copyright 2011 the V8 project authors. All rights reserved.
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

// Regression test for Chromium issue 70066.  Delete should work properly
// from inside 'with' scopes.
// http://code.google.com/p/chromium/issues/detail?id=70066

x = 0;

// Delete on a slot from a function's own context.
function test1() {
  var value = 1;
  var status;
  with ({}) { status = delete value; }
  return value + ":" + status;
}

assertEquals("1:false", test1(), "test1");
assertEquals(0, x, "test1");  // Global x is undisturbed.


// Delete on a slot from an outer context.
function test2() {
  function f() {
    with ({}) { return delete value; }
  }
  var value = 2;
  var status = f();
  return value + ":" + status;
}

assertEquals("2:false", test2(), "test2");
assertEquals(0, x, "test2");  // Global x is undisturbed.


// Delete on an argument.  This hits the same code paths as test5 because
// 'with' forces all parameters to be indirected through the arguments
// object.
function test3(value) {
  var status;
  with ({}) { status = delete value; }
  return value + ":" + status;
}

assertEquals("undefined:true", test3(3), "test3");
assertEquals(0, x, "test3");  // Global x is undisturbed.


// Delete on an argument from an outer context.  This hits the same code
// path as test2.
function test4(value) {
  function f() {
    with ({}) { return delete value; }
  }
  var status = f();
  return value + ":" + status;
}

assertEquals("4:false", test4(4), "test4");
assertEquals(0, x, "test4");  // Global x is undisturbed.


// Delete on an argument found in the arguments object.  Such properties are
// normally DONT_DELETE in JavaScript but deletion is allowed by V8.
function test5(value) {
  var status;
  with ({}) { status = delete value; }
  return arguments[0] + ":" + status;
}

assertEquals("undefined:true", test5(5), "test5");
assertEquals(0, x, "test5");  // Global x is undisturbed.

function test6(value) {
  function f() {
    with ({}) { return delete value; }
  }
  var status = f();
  return arguments[0] + ":" + status;
}

assertEquals("undefined:true", test6(6), "test6");
assertEquals(0, x, "test6");  // Global x is undisturbed.


// Delete on a property found on 'with' object.
function test7(object) {
  with (object) { return delete value; }
}

var o = {value: 7};
assertEquals(true, test7(o), "test7");
assertEquals(void 0, o.value, "test7");
assertEquals(0, x, "test7");  // Global x is undisturbed.


// Delete on a global property.
function test8() {
  with ({}) { return delete x; }
}

assertEquals(true, test8(), "test8");
assertThrows("x", "test8");  // Global x should be deleted.


// Delete on a property that is not found anywhere.
function test9() {
  with ({}) { return delete x; }
}

assertThrows("x", "test9");  // Make sure it's not there.
assertEquals(true, test9(), "test9");


// Delete on a DONT_DELETE property of the global object.
var y = 10;
function test10() {
  with ({}) { return delete y; }
}

assertEquals(false, test10(), "test10");
assertEquals(10, y, "test10");
