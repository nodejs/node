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

function CheckStrictMode(code, exception) {
  assertDoesNotThrow(code);
  assertThrows("'use strict';\n" + code, exception);
  assertThrows('"use strict";\n' + code, exception);
  assertDoesNotThrow("\
    function outer() {\
      function inner() {\n"
        + code +
      "\n}\
    }");
  assertThrows("\
    function outer() {\
      'use strict';\
      function inner() {\n"
        + code +
      "\n}\
    }", exception);
}

function CheckFunctionConstructorStrictMode() {
  var args = [];
  for (var i = 0; i < arguments.length; i ++) {
    args[i] = arguments[i];
  }
  // Create non-strict function. No exception.
  args[arguments.length] = "";
  assertDoesNotThrow(function() {
    Function.apply(this, args);
  });
  // Create strict mode function. Exception expected.
  args[arguments.length] = "'use strict';";
  assertThrows(function() {
    Function.apply(this, args);
  }, SyntaxError);
}

// Incorrect 'use strict' directive.
(function UseStrictEscape() {
  "use\\x20strict";
  with ({}) {};
})();

// 'use strict' in non-directive position.
(function UseStrictNonDirective() {
  void(0);
  "use strict";
  with ({}) {};
})();

// Multiple directives, including "use strict".
assertThrows('\
"directive 1";\
"another directive";\
"use strict";\
"directive after strict";\
"and one more";\
with({}) {}', SyntaxError);

// 'with' disallowed in strict mode.
CheckStrictMode("with({}) {}", SyntaxError);

// Function named 'eval'.
CheckStrictMode("function eval() {}", SyntaxError);

// Function named 'arguments'.
CheckStrictMode("function arguments() {}", SyntaxError);

// Function parameter named 'eval'.
CheckStrictMode("function foo(a, b, eval, c, d) {}", SyntaxError);

// Function parameter named 'arguments'.
CheckStrictMode("function foo(a, b, arguments, c, d) {}", SyntaxError);

// Property accessor parameter named 'eval'.
CheckStrictMode("var o = { set foo(eval) {} }", SyntaxError);

// Property accessor parameter named 'arguments'.
CheckStrictMode("var o = { set foo(arguments) {} }", SyntaxError);

// Duplicate function parameter name.
CheckStrictMode("function foo(a, b, c, d, b) {}", SyntaxError);

// Function constructor: eval parameter name.
CheckFunctionConstructorStrictMode("eval");

// Function constructor: arguments parameter name.
CheckFunctionConstructorStrictMode("arguments");

// Function constructor: duplicate parameter name.
CheckFunctionConstructorStrictMode("a", "b", "c", "b");
CheckFunctionConstructorStrictMode("a,b,c,b");

// catch(eval)
CheckStrictMode("try{}catch(eval){};", SyntaxError);

// catch(arguments)
CheckStrictMode("try{}catch(arguments){};", SyntaxError);

// var eval
CheckStrictMode("var eval;", SyntaxError);

// var arguments
CheckStrictMode("var arguments;", SyntaxError);

// Strict mode applies to the function in which the directive is used..
assertThrows('\
function foo(eval) {\
  "use strict";\
}', SyntaxError);

// Strict mode doesn't affect the outer stop of strict code.
(function NotStrict(eval) {
  function Strict() {
    "use strict";
  }
  with ({}) {};
})();

// Octal literal
CheckStrictMode("var x = 012");
CheckStrictMode("012");
CheckStrictMode("'Hello octal\\032'");
CheckStrictMode("function octal() { return 012; }");
CheckStrictMode("function octal() { return '\\032'; }");

(function ValidEscape() {
  "use strict";
  var x = '\0';
  var y = "\0";
})();

// Octal before "use strict"
assertThrows('\
  function strict() {\
    "octal\\032directive";\
    "use strict";\
  }', SyntaxError);

// Duplicate data properties.
CheckStrictMode("var x = { dupe : 1, nondupe: 3, dupe : 2 };", SyntaxError);
CheckStrictMode("var x = { '1234' : 1, '2345' : 2, '1234' : 3 };", SyntaxError);
CheckStrictMode("var x = { '1234' : 1, '2345' : 2, 1234 : 3 };", SyntaxError);
CheckStrictMode("var x = { 3.14 : 1, 2.71 : 2, 3.14 : 3 };", SyntaxError);
CheckStrictMode("var x = { 3.14 : 1, '3.14' : 2 };", SyntaxError);
CheckStrictMode("var x = { \
  123: 1, \
  123.00000000000000000000000000000000000000000000000000000000000000000001: 2 \
}", SyntaxError);

// Non-conflicting data properties.
(function StrictModeNonDuplicate() {
  "use strict";
  var x = { 123 : 1, "0123" : 2 };
  var x = {
    123: 1,
    '123.00000000000000000000000000000000000000000000000000000000000000000001':
      2
  };
})();

// Two getters (non-strict)
assertThrows("var x = { get foo() { }, get foo() { } };", SyntaxError);
assertThrows("var x = { get foo(){}, get 'foo'(){}};", SyntaxError);
assertThrows("var x = { get 12(){}, get '12'(){}};", SyntaxError);

// Two setters (non-strict)
assertThrows("var x = { set foo(v) { }, set foo(v) { } };", SyntaxError);
assertThrows("var x = { set foo(v) { }, set 'foo'(v) { } };", SyntaxError);
assertThrows("var x = { set 13(v) { }, set '13'(v) { } };", SyntaxError);

// Setter and data (non-strict)
assertThrows("var x = { foo: 'data', set foo(v) { } };", SyntaxError);
assertThrows("var x = { set foo(v) { }, foo: 'data' };", SyntaxError);
assertThrows("var x = { foo: 'data', set 'foo'(v) { } };", SyntaxError);
assertThrows("var x = { set foo(v) { }, 'foo': 'data' };", SyntaxError);
assertThrows("var x = { 'foo': 'data', set foo(v) { } };", SyntaxError);
assertThrows("var x = { set 'foo'(v) { }, foo: 'data' };", SyntaxError);
assertThrows("var x = { 'foo': 'data', set 'foo'(v) { } };", SyntaxError);
assertThrows("var x = { set 'foo'(v) { }, 'foo': 'data' };", SyntaxError);
assertThrows("var x = { 12: 1, set '12'(v){}};", SyntaxError);
assertThrows("var x = { 12: 1, set 12(v){}};", SyntaxError);
assertThrows("var x = { '12': 1, set '12'(v){}};", SyntaxError);
assertThrows("var x = { '12': 1, set 12(v){}};", SyntaxError);

// Getter and data (non-strict)
assertThrows("var x = { foo: 'data', get foo() { } };", SyntaxError);
assertThrows("var x = { get foo() { }, foo: 'data' };", SyntaxError);
assertThrows("var x = { 'foo': 'data', get foo() { } };", SyntaxError);
assertThrows("var x = { get 'foo'() { }, 'foo': 'data' };", SyntaxError);
assertThrows("var x = { '12': 1, get '12'(){}};", SyntaxError);
assertThrows("var x = { '12': 1, get 12(){}};", SyntaxError);

// Assignment to eval or arguments
CheckStrictMode("function strict() { eval = undefined; }", SyntaxError);
CheckStrictMode("function strict() { arguments = undefined; }", SyntaxError);
CheckStrictMode("function strict() { print(eval = undefined); }", SyntaxError);
CheckStrictMode("function strict() { print(arguments = undefined); }",
                SyntaxError);
CheckStrictMode("function strict() { var x = eval = undefined; }", SyntaxError);
CheckStrictMode("function strict() { var x = arguments = undefined; }",
                SyntaxError);

// Compound assignment to eval or arguments
CheckStrictMode("function strict() { eval *= undefined; }", SyntaxError);
CheckStrictMode("function strict() { arguments /= undefined; }", SyntaxError);
CheckStrictMode("function strict() { print(eval %= undefined); }", SyntaxError);
CheckStrictMode("function strict() { print(arguments %= undefined); }",
                SyntaxError);
CheckStrictMode("function strict() { var x = eval += undefined; }",
                SyntaxError);
CheckStrictMode("function strict() { var x = arguments -= undefined; }",
                SyntaxError);
CheckStrictMode("function strict() { eval <<= undefined; }", SyntaxError);
CheckStrictMode("function strict() { arguments >>= undefined; }", SyntaxError);
CheckStrictMode("function strict() { print(eval >>>= undefined); }",
                SyntaxError);
CheckStrictMode("function strict() { print(arguments &= undefined); }",
                SyntaxError);
CheckStrictMode("function strict() { var x = eval ^= undefined; }",
                SyntaxError);
CheckStrictMode("function strict() { var x = arguments |= undefined; }",
                SyntaxError);

// Postfix increment with eval or arguments
CheckStrictMode("function strict() { eval++; }", SyntaxError);
CheckStrictMode("function strict() { arguments++; }", SyntaxError);
CheckStrictMode("function strict() { print(eval++); }", SyntaxError);
CheckStrictMode("function strict() { print(arguments++); }", SyntaxError);
CheckStrictMode("function strict() { var x = eval++; }", SyntaxError);
CheckStrictMode("function strict() { var x = arguments++; }", SyntaxError);

// Postfix decrement with eval or arguments
CheckStrictMode("function strict() { eval--; }", SyntaxError);
CheckStrictMode("function strict() { arguments--; }", SyntaxError);
CheckStrictMode("function strict() { print(eval--); }", SyntaxError);
CheckStrictMode("function strict() { print(arguments--); }", SyntaxError);
CheckStrictMode("function strict() { var x = eval--; }", SyntaxError);
CheckStrictMode("function strict() { var x = arguments--; }", SyntaxError);

// Prefix increment with eval or arguments
CheckStrictMode("function strict() { ++eval; }", SyntaxError);
CheckStrictMode("function strict() { ++arguments; }", SyntaxError);
CheckStrictMode("function strict() { print(++eval); }", SyntaxError);
CheckStrictMode("function strict() { print(++arguments); }", SyntaxError);
CheckStrictMode("function strict() { var x = ++eval; }", SyntaxError);
CheckStrictMode("function strict() { var x = ++arguments; }", SyntaxError);

// Prefix decrement with eval or arguments
CheckStrictMode("function strict() { --eval; }", SyntaxError);
CheckStrictMode("function strict() { --arguments; }", SyntaxError);
CheckStrictMode("function strict() { print(--eval); }", SyntaxError);
CheckStrictMode("function strict() { print(--arguments); }", SyntaxError);
CheckStrictMode("function strict() { var x = --eval; }", SyntaxError);
CheckStrictMode("function strict() { var x = --arguments; }", SyntaxError);

// Delete of an unqualified identifier
CheckStrictMode("delete unqualified;", SyntaxError);
CheckStrictMode("function strict() { delete unqualified; }", SyntaxError);
CheckStrictMode("function function_name() { delete function_name; }",
                SyntaxError);
CheckStrictMode("function strict(parameter) { delete parameter; }",
                SyntaxError);
CheckStrictMode("function strict() { var variable; delete variable; }",
                SyntaxError);
CheckStrictMode("var variable; delete variable;", SyntaxError);

// Prefix unary operators other than delete, ++, -- are valid in strict mode
(function StrictModeUnaryOperators() {
  "use strict";
  var x = [void eval, typeof eval, +eval, -eval, ~eval, !eval];
  var y = [void arguments, typeof arguments,
           +arguments, -arguments, ~arguments, !arguments];
})();

// 7.6.1.2 Future Reserved Words
var future_reserved_words = [
  "class",
  "enum",
  "export",
  "extends",
  "import",
  "super",
  "implements",
  "interface",
  "let",
  "package",
  "private",
  "protected",
  "public",
  "static",
  "yield" ];

function testFutureReservedWord(word) {
  // Simple use of each reserved word
  CheckStrictMode("var " + word + " = 1;", SyntaxError);

  // object literal properties
  eval("var x = { " + word + " : 42 };");
  eval("var x = { get " + word + " () {} };");
  eval("var x = { set " + word + " (value) {} };");

  // object literal with string literal property names
  eval("var x = { '" + word + "' : 42 };");
  eval("var x = { get '" + word + "' () { } };");
  eval("var x = { set '" + word + "' (value) { } };");
  eval("var x = { get '" + word + "' () { 'use strict'; } };");
  eval("var x = { set '" + word + "' (value) { 'use strict'; } };");

  // Function names and arguments, strict and non-strict contexts
  CheckStrictMode("function " + word + " () {}", SyntaxError);
  CheckStrictMode("function foo (" + word + ") {}", SyntaxError);
  CheckStrictMode("function foo (" + word + ", " + word + ") {}", SyntaxError);
  CheckStrictMode("function foo (a, " + word + ") {}", SyntaxError);
  CheckStrictMode("function foo (" + word + ", a) {}", SyntaxError);
  CheckStrictMode("function foo (a, " + word + ", b) {}", SyntaxError);
  CheckStrictMode("var foo = function (" + word + ") {}", SyntaxError);

  // Function names and arguments when the body is strict
  assertThrows("function " + word + " () { 'use strict'; }", SyntaxError);
  assertThrows("function foo (" + word + ")  'use strict'; {}", SyntaxError);
  assertThrows("function foo (" + word + ", " + word + ") { 'use strict'; }",
               SyntaxError);
  assertThrows("function foo (a, " + word + ") { 'use strict'; }", SyntaxError);
  assertThrows("function foo (" + word + ", a) { 'use strict'; }", SyntaxError);
  assertThrows("function foo (a, " + word + ", b) { 'use strict'; }",
               SyntaxError);
  assertThrows("var foo = function (" + word + ") { 'use strict'; }",
               SyntaxError);

  // get/set when the body is strict
  eval("var x = { get " + word + " () { 'use strict'; } };");
  eval("var x = { set " + word + " (value) { 'use strict'; } };");
  assertThrows("var x = { get foo(" + word + ") { 'use strict'; } };",
               SyntaxError);
  assertThrows("var x = { set foo(" + word + ") { 'use strict'; } };",
               SyntaxError);
}

for (var i = 0; i < future_reserved_words.length; i++) {
  testFutureReservedWord(future_reserved_words[i]);
}

function testAssignToUndefined(should_throw) {
  "use strict";
  try {
    possibly_undefined_variable_for_strict_mode_test = "should throw?";
  } catch (e) {
    assertTrue(should_throw, "strict mode");
    assertInstanceof(e, ReferenceError, "strict mode");
    return;
  }
  assertFalse(should_throw, "strict mode");
}

testAssignToUndefined(true);
testAssignToUndefined(true);
testAssignToUndefined(true);

possibly_undefined_variable_for_strict_mode_test = "value";

testAssignToUndefined(false);
testAssignToUndefined(false);
testAssignToUndefined(false);

delete possibly_undefined_variable_for_strict_mode_test;

testAssignToUndefined(true);
testAssignToUndefined(true);
testAssignToUndefined(true);

function repeat(n, f) {
 for (var i = 0; i < n; i ++) { f(); }
}

repeat(10, function() { testAssignToUndefined(true); });
possibly_undefined_variable_for_strict_mode_test = "value";
repeat(10, function() { testAssignToUndefined(false); });
delete possibly_undefined_variable_for_strict_mode_test;
repeat(10, function() { testAssignToUndefined(true); });
possibly_undefined_variable_for_strict_mode_test = undefined;
repeat(10, function() { testAssignToUndefined(false); });

(function testDeleteNonConfigurable() {
  function delete_property(o) {
    "use strict";
    delete o.property;
  }
  function delete_element(o, i) {
    "use strict";
    delete o[i];
  }

  var object = {};

  Object.defineProperty(object, "property", { value: "property_value" });
  Object.defineProperty(object, "1", { value: "one" });
  Object.defineProperty(object, 7, { value: "seven" });
  Object.defineProperty(object, 3.14, { value: "pi" });

  assertThrows(function() { delete_property(object); }, TypeError);
  assertEquals(object.property, "property_value");
  assertThrows(function() { delete_element(object, "1"); }, TypeError);
  assertThrows(function() { delete_element(object, 1); }, TypeError);
  assertEquals(object[1], "one");
  assertThrows(function() { delete_element(object, "7"); }, TypeError);
  assertThrows(function() { delete_element(object, 7); }, TypeError);
  assertEquals(object[7], "seven");
  assertThrows(function() { delete_element(object, "3.14"); }, TypeError);
  assertThrows(function() { delete_element(object, 3.14); }, TypeError);
  assertEquals(object[3.14], "pi");
})();

// Not transforming this in Function.call and Function.apply.
(function testThisTransform() {
  function non_strict() {
    return this;
  }
  function strict() {
    "use strict";
    return this;
  }

  var global_object = (function() { return this; })();
  var object = {};

  // Non-strict call.
  assertTrue(non_strict.call(null) === global_object);
  assertTrue(non_strict.call(undefined) === global_object);
  assertEquals(typeof non_strict.call(7), "object");
  assertEquals(typeof non_strict.call("Hello"), "object");
  assertTrue(non_strict.call(object) === object);

  // Non-strict apply.
  assertTrue(non_strict.apply(null) === global_object);
  assertTrue(non_strict.apply(undefined) === global_object);
  assertEquals(typeof non_strict.apply(7), "object");
  assertEquals(typeof non_strict.apply("Hello"), "object");
  assertTrue(non_strict.apply(object) === object);

  // Strict call.
  assertTrue(strict.call(null) === null);
  assertTrue(strict.call(undefined) === undefined);
  assertEquals(typeof strict.call(7), "number");
  assertEquals(typeof strict.call("Hello"), "string");
  assertTrue(strict.call(object) === object);

  // Strict apply.
  assertTrue(strict.apply(null) === null);
  assertTrue(strict.apply(undefined) === undefined);
  assertEquals(typeof strict.apply(7), "number");
  assertEquals(typeof strict.apply("Hello"), "string");
  assertTrue(strict.apply(object) === object);
})();
