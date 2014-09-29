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

function testEvalWithSourceURL() {
  eval("function Doo() { FAIL; }; Doo();\n//# sourceURL=res://name");
}

function testNestedEvalWithSourceURL() {
  var x = "FAIL";
  var innerEval = 'function Inner() { eval(x); }\n//@ sourceURL=res://inner-eval';
  eval("function Outer() { eval(innerEval); Inner(); }; Outer();\n//# sourceURL=res://outer-eval");
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

MyObj = function() { FAIL; }

MyObjCreator = function() {}

MyObjCreator.prototype.Create = function() {
  return new MyObj();
}

function testClassNames() {
  (new MyObjCreator).Create();
}

// Utility function for testing that the expected strings occur
// in the stack trace produced when running the given function.
function testTrace(name, fun, expected, unexpected) {
  var threw = false;
  try {
    fun();
  } catch (e) {
    for (var i = 0; i < expected.length; i++) {
      assertTrue(e.stack.indexOf(expected[i]) != -1,
                 name + " doesn't contain expected[" + i + "] stack = " + e.stack);
    }
    if (unexpected) {
      for (var i = 0; i < unexpected.length; i++) {
        assertEquals(e.stack.indexOf(unexpected[i]), -1,
                     name + " contains unexpected[" + i + "]");
      }
    }
    threw = true;
  }
  assertTrue(threw, name + " didn't throw");
}

// Test that the error constructor is not shown in the trace
function testCallerCensorship() {
  var threw = false;
  try {
    FAIL;
  } catch (e) {
    assertEquals(-1, e.stack.indexOf('at new ReferenceError'),
                 "CallerCensorship contained new ReferenceError");
    threw = true;
  }
  assertTrue(threw, "CallerCensorship didn't throw");
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
    assertTrue(e.stack.indexOf('at new ReferenceError') != -1,
               "UnintendedCallerCensorship didn't contain new ReferenceError");
    threw = true;
  }
  assertTrue(threw, "UnintendedCallerCensorship didn't throw");
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
    assertTrue(e.stack.indexOf('<error: ReferenceError') != -1,
               "ErrorsDuringFormatting didn't contain error: ReferenceError");
  }
  assertTrue(threw, "ErrorsDuringFormatting didn't throw");
  threw = false;
  // Now we can't even format the message saying that we couldn't format
  // the stack frame.  Put that in your pipe and smoke it!
  ReferenceError.prototype.toString = function () { NESTED_FAIL; };
  try {
    n.foo();
  } catch (e) {
    threw = true;
    assertTrue(e.stack.indexOf('<error>') != -1,
               "ErrorsDuringFormatting didn't contain <error>");
  }
  assertTrue(threw, "ErrorsDuringFormatting didnt' throw (2)");
}


// Poisonous object that throws a reference error if attempted converted to
// a primitive values.
var thrower = { valueOf: function() { FAIL; },
                toString: function() { FAIL; } };

// Tests that a native constructor function is included in the
// stack trace.
function testTraceNativeConstructor(nativeFunc) {
  var nativeFuncName = nativeFunc.name;
  try {
    new nativeFunc(thrower);
    assertUnreachable(nativeFuncName);
  } catch (e) {
    assertTrue(e.stack.indexOf(nativeFuncName) >= 0, nativeFuncName);
  }
}

// Tests that a native conversion function is included in the
// stack trace.
function testTraceNativeConversion(nativeFunc) {
  var nativeFuncName = nativeFunc.name;
  try {
    nativeFunc(thrower);
    assertUnreachable(nativeFuncName);
  } catch (e) {
    assertTrue(e.stack.indexOf(nativeFuncName) >= 0, nativeFuncName);
  }
}


function testOmittedBuiltin(throwing, omitted) {
  try {
    throwing();
    assertUnreachable(omitted);
  } catch (e) {
    assertTrue(e.stack.indexOf(omitted) < 0, omitted);
  }
}


testTrace("testArrayNative", testArrayNative, ["Array.map (native)"]);
testTrace("testNested", testNested, ["at one", "at two", "at three"]);
testTrace("testMethodNameInference", testMethodNameInference, ["at Foo.bar"]);
testTrace("testImplicitConversion", testImplicitConversion, ["at Nirk.valueOf"]);
testTrace("testEval", testEval, ["at Doo (eval at testEval"]);
testTrace("testNestedEval", testNestedEval, ["eval at Inner (eval at Outer"]);
testTrace("testEvalWithSourceURL", testEvalWithSourceURL,
    [ "at Doo (res://name:1:18)" ]);
testTrace("testNestedEvalWithSourceURL", testNestedEvalWithSourceURL,
    [" at Inner (res://inner-eval:1:20)",
     " at Outer (res://outer-eval:1:37)"]);
testTrace("testValue", testValue, ["at Number.causeError"]);
testTrace("testConstructor", testConstructor, ["new Plonk"]);
testTrace("testRenamedMethod", testRenamedMethod, ["Wookie.a$b$c$d [as d]"]);
testTrace("testAnonymousMethod", testAnonymousMethod, ["Array.<anonymous>"]);
testTrace("testDefaultCustomError", testDefaultCustomError,
    ["hep-hey", "new CustomError"],
    ["collectStackTrace"]);
testTrace("testStrippedCustomError", testStrippedCustomError, ["hep-hey"],
    ["new CustomError", "collectStackTrace"]);
testTrace("testClassNames", testClassNames,
          ["new MyObj", "MyObjCreator.Create"], ["as Create"]);
testCallerCensorship();
testUnintendedCallerCensorship();
testErrorsDuringFormatting();

testTraceNativeConversion(String);  // Does ToString on argument.
testTraceNativeConversion(Number);  // Does ToNumber on argument.
testTraceNativeConversion(RegExp);  // Does ToString on argument.

testTraceNativeConstructor(String);  // Does ToString on argument.
testTraceNativeConstructor(Number);  // Does ToNumber on argument.
testTraceNativeConstructor(RegExp);  // Does ToString on argument.
testTraceNativeConstructor(Date);    // Does ToNumber on argument.

// Omitted because QuickSort has builtins object as receiver, and is non-native
// builtin.
testOmittedBuiltin(function(){ [thrower, 2].sort(function (a,b) {
                                                     (b < a) - (a < b); });
                   }, "QuickSort");

// Omitted because ADD from runtime.js is non-native builtin.
testOmittedBuiltin(function(){ thrower + 2; }, "ADD");

var error = new Error();
error.toString = function() { assertUnreachable(); };
error.stack;

error = new Error();
error.name = { toString: function() { assertUnreachable(); }};
error.message = { toString: function() {  assertUnreachable(); }};
error.stack;

error = new Error();
Array.prototype.push = function(x) { assertUnreachable(); };
Array.prototype.join = function(x) { assertUnreachable(); };
error.stack;

var fired = false;
error = new Error({ toString: function() { fired = true; } });
assertTrue(fired);
error.stack;
assertTrue(fired);

// Check that throwing exception in a custom stack trace formatting function
// does not lead to recursion.
Error.prepareStackTrace = function() { throw new Error("abc"); };
var message;
try {
  try {
    throw new Error();
  } catch (e) {
    e.stack;
  }
} catch (e) {
  message = e.message;
}

assertEquals("abc", message);

// Test that modifying Error.prepareStackTrace by itself works.
Error.prepareStackTrace = function() { Error.prepareStackTrace = "custom"; };
new Error().stack;

assertEquals("custom", Error.prepareStackTrace);

// Check that the formatted stack trace can be set to undefined.
error = new Error();
error.stack = undefined;
assertEquals(undefined, error.stack);

// Check that the stack trace accessors are not forcibly set.
var my_error = {};
Object.freeze(my_error);
assertThrows(function() { Error.captureStackTrace(my_error); });

my_error = {};
Object.preventExtensions(my_error);
assertThrows(function() { Error.captureStackTrace(my_error); });

var fake_error = {};
my_error = new Error();
var stolen_getter = Object.getOwnPropertyDescriptor(my_error, 'stack').get;
Object.defineProperty(fake_error, 'stack', { get: stolen_getter });
assertEquals(undefined, fake_error.stack);
