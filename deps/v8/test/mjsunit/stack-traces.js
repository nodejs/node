// Copyright 2009 the V8 project authors. All rights reserved.
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

function testMethodNameInference() {
  function Foo() { }
  Foo.prototype.bar = function () { FAIL; };
  (new Foo).bar();
}

function testNested() {
  function one() {
    function two() {
      function three() {
        FAIL;
      }
      three();
    }
    two();
  }
  one();
}

function testArrayNative() {
  [1, 2, 3].map(function () { FAIL; });
}

function testImplicitConversion() {
  function Nirk() { }
  Nirk.prototype.valueOf = function () { FAIL; };
  return 1 + (new Nirk);
}

function testEval() {
  eval("function Doo() { FAIL; }; Doo();");
}

function testNestedEval() {
  var x = "FAIL";
  eval("function Outer() { eval('function Inner() { eval(x); }'); Inner(); }; Outer();");
}

function testValue() {
  Number.prototype.causeError = function () { FAIL; };
  (1).causeError();
}

function testConstructor() {
  function Plonk() { FAIL; }
  new Plonk();
}

function testRenamedMethod() {
  function a$b$c$d() { return FAIL; }
  function Wookie() { }
  Wookie.prototype.d = a$b$c$d;
  (new Wookie).d();
}

function testAnonymousMethod() {
  (function () { FAIL }).call([1, 2, 3]);
}

function CustomError(message, stripPoint) {
  this.message = message;
  Error.captureStackTrace(this, stripPoint);
}

CustomError.prototype.toString = function () {
  return "CustomError: " + this.message;
};

function testDefaultCustomError() {
  throw new CustomError("hep-hey", undefined);
}

function testStrippedCustomError() {
  throw new CustomError("hep-hey", CustomError);
}

// Utility function for testing that the expected strings occur
// in the stack trace produced when running the given function.
function testTrace(fun, expected, unexpected) {
  var threw = false;
  try {
    fun();
  } catch (e) {
    for (var i = 0; i < expected.length; i++) {
      assertTrue(e.stack.indexOf(expected[i]) != -1);
    }
    if (unexpected) {
      for (var i = 0; i < unexpected.length; i++) {
        assertEquals(e.stack.indexOf(unexpected[i]), -1);
      }
    }
    threw = true;
  }
  assertTrue(threw);
}

// Test that the error constructor is not shown in the trace
function testCallerCensorship() {
  var threw = false;
  try {
    FAIL;
  } catch (e) {
    assertEquals(-1, e.stack.indexOf('at new ReferenceError'));
    threw = true;
  }
  assertTrue(threw);
}

// Test that the explicit constructor call is shown in the trace
function testUnintendedCallerCensorship() {
  var threw = false;
  try {
    new ReferenceError({
      toString: function () {
        FAIL;
      }
    });
  } catch (e) {
    assertTrue(e.stack.indexOf('at new ReferenceError') != -1);
    threw = true;
  }
  assertTrue(threw);
}

// If an error occurs while the stack trace is being formatted it should
// be handled gracefully.
function testErrorsDuringFormatting() {
  function Nasty() { }
  Nasty.prototype.foo = function () { throw new RangeError(); };
  var n = new Nasty();
  n.__defineGetter__('constructor', function () { CONS_FAIL; });
  var threw = false;
  try {
    n.foo();
  } catch (e) {
    threw = true;
    assertTrue(e.stack.indexOf('<error: ReferenceError') != -1);
  }
  assertTrue(threw);
  threw = false;
  // Now we can't even format the message saying that we couldn't format
  // the stack frame.  Put that in your pipe and smoke it!
  ReferenceError.prototype.toString = function () { NESTED_FAIL; };
  try {
    n.foo();
  } catch (e) {
    threw = true;
    assertTrue(e.stack.indexOf('<error>') != -1);
  }
  assertTrue(threw);
}

testTrace(testArrayNative, ["Array.map (native)"]);
testTrace(testNested, ["at one", "at two", "at three"]);
testTrace(testMethodNameInference, ["at Foo.bar"]);
testTrace(testImplicitConversion, ["at Nirk.valueOf"]);
testTrace(testEval, ["at Doo (eval at testEval"]);
testTrace(testNestedEval, ["eval at Inner (eval at Outer"]);
testTrace(testValue, ["at Number.causeError"]);
testTrace(testConstructor, ["new Plonk"]);
testTrace(testRenamedMethod, ["Wookie.a$b$c$d [as d]"]);
testTrace(testAnonymousMethod, ["Array.<anonymous>"]);
testTrace(testDefaultCustomError, ["hep-hey", "new CustomError"],
    ["collectStackTrace"]);
testTrace(testStrippedCustomError, ["hep-hey"], ["new CustomError",
    "collectStackTrace"]);

testCallerCensorship();
testUnintendedCallerCensorship();
testErrorsDuringFormatting();
