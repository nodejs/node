// Copyright 2014 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --strong-mode --harmony-sloppy

// Repurposing the strict mode 'eval' and 'arguments' tests to test for correct
// behaviour of 'undefined' as an identifier in strong mode.
"use strict";

function CheckStrongMode(code) {
  let strictContexts = [
    ["'use strict';", ""],
    ["function outer() { 'use strict';", "}"],
    ["function outer() { 'use strict'; function inner() {", "}}"],
    ["class C { m() {", "} }"]
  ]
  let strongContexts = [
    ["'use strong';", ""],
    ["function outer() { 'use strong';", "}"],
    ["function outer() { 'use strong'; function inner() {", "}}"],
    ["class C { m() { 'use strong';", "} }"]
  ]

  for (let context of strictContexts) {
    assertThrows(context[0] + code + context[1] + "; throw new TypeError();",
                 TypeError);
  }
  for (let context of strongContexts) {
    assertThrows(context[0] + code + context[1], SyntaxError);
  }
}

// Binding 'undefined'
CheckStrongMode("var undefined;");
CheckStrongMode("let undefined;");
CheckStrongMode("var undefined = 0;");
CheckStrongMode("let undefined = 0;");
CheckStrongMode("const undefined = 0;");
CheckStrongMode("var x, y = 0, undefined;");
CheckStrongMode("let x, y = 0, undefined;");

// Function identifier is 'undefined'
// Function declaration
CheckStrongMode("function undefined() {}");
assertThrows("function undefined() {'use strong';}", SyntaxError);

// Generator function
CheckStrongMode("function* undefined() {}");
assertThrows("function* undefined() {'use strong';}", SyntaxError);

// Function expression
CheckStrongMode("(function undefined() {});");
assertThrows("(function undefined() {'use strong';});", SyntaxError);
CheckStrongMode("{foo: (function undefined(){})};");
assertThrows("{foo: (function undefined(){'use strong';})};", SyntaxError);

//Generator function expression
CheckStrongMode("(function* undefined() {})");
assertThrows("(function* undefined() {'use strong';})", SyntaxError);
CheckStrongMode("{foo: (function* undefined(){})};");
assertThrows("{foo: (function* undefined(){'use strong';})};", SyntaxError);

// Function parameter named 'undefined'
// Function declaration
CheckStrongMode("function foo(a, b, undefined, c, d) {}");
assertThrows("function foo(a, b, undefined, c, d) {'use strong';}",
             SyntaxError);

// Generator function declaration
CheckStrongMode("function* foo(a, b, undefined, c, d) {}");
assertThrows("function* foo(a, b, undefined, c, d) {'use strong';}",
             SyntaxError);

// Function expression
CheckStrongMode("(function foo(a, b, undefined, c, d) {});");
assertThrows("(function foo(a, b, undefined, c, d) {'use strong';})",
             SyntaxError);
CheckStrongMode("{foo: (function foo(a, b, undefined, c, d) {})};");
assertThrows("{foo: (function foo(a, b, undefined, c, d) {'use strong';})};",
             SyntaxError);

// Generator function expression
CheckStrongMode("(function* foo(a, b, undefined, c, d) {});");
assertThrows("(function* foo(a, b, undefined, c, d) {'use strong';})",
             SyntaxError);
CheckStrongMode("{foo: (function* foo(a, b, undefined, c, d) {})};");
assertThrows("{foo: (function* foo(a, b, undefined, c, d) {'use strong';})};",
             SyntaxError);

// Method parameter named 'undefined'
// Class method
CheckStrongMode("class C { foo(a, b, undefined, c, d) {} }");
assertThrows("class C { foo(a, b, undefined, c, d) {'use strong';} }",
             SyntaxError);

//Class generator method
CheckStrongMode("class C { *foo(a, b, undefined, c, d) {} }");
assertThrows("class C { *foo(a, b, undefined, c, d) {'use strong';} }",
             SyntaxError);

//Object literal method
CheckStrongMode("({ foo(a, b, undefined, c, d) {} });");
assertThrows("({ foo(a, b, undefined, c, d) {'use strong';} });", SyntaxError);

//Object literal generator method
CheckStrongMode("({ *foo(a, b, undefined, c, d) {} });");
assertThrows("({ *foo(a, b, undefined, c, d) {'use strong';} });", SyntaxError);

// Class declaration named 'undefined'
CheckStrongMode("class undefined {}");
assertThrows("class undefined {'use strong'}", SyntaxError);

// Class expression named 'undefined'
CheckStrongMode("(class undefined {});");
assertThrows("(class undefined {'use strong'});", SyntaxError);

// Binding/assigning to 'undefined' in for
CheckStrongMode("for(undefined = 0;false;);");
CheckStrongMode("for(var undefined = 0;false;);");
CheckStrongMode("for(let undefined = 0;false;);");
CheckStrongMode("for(const undefined = 0;false;);");

// Binding/assigning to 'undefined' in for-in
CheckStrongMode("for(undefined in {});");
CheckStrongMode("for(var undefined in {});");
CheckStrongMode("for(let undefined in {});");
CheckStrongMode("for(const undefined in {});");

// Binding/assigning to 'undefined' in for-of
CheckStrongMode("for(undefined of []);");
CheckStrongMode("for(var undefined of []);");
CheckStrongMode("for(let undefined of []);");
CheckStrongMode("for(const undefined of []);");

// Property accessor parameter named 'undefined'.
CheckStrongMode("let o = { set foo(undefined) {} }");
assertThrows("let o = { set foo(undefined) {'use strong';} }", SyntaxError);

// catch(undefined)
CheckStrongMode("try {} catch(undefined) {};");

// Assignment to undefined
CheckStrongMode("undefined = 0;");
CheckStrongMode("print(undefined = 0);");
CheckStrongMode("let x = undefined = 0;");

// Compound assignment to undefined
CheckStrongMode("undefined *= 0;");
CheckStrongMode("undefined /= 0;");
CheckStrongMode("print(undefined %= 0);");
CheckStrongMode("let x = undefined += 0;");
CheckStrongMode("let x = undefined -= 0;");
CheckStrongMode("undefined <<= 0;");
CheckStrongMode("undefined >>= 0;");
CheckStrongMode("print(undefined >>>= 0);");
CheckStrongMode("print(undefined &= 0);");
CheckStrongMode("let x = undefined ^= 0;");
CheckStrongMode("let x = undefined |= 0;");

// Postfix increment with undefined
CheckStrongMode("undefined++;");
CheckStrongMode("print(undefined++);");
CheckStrongMode("let x = undefined++;");

// Postfix decrement with undefined
CheckStrongMode("undefined--;");
CheckStrongMode("print(undefined--);");
CheckStrongMode("let x = undefined--;");

// Prefix increment with undefined
CheckStrongMode("++undefined;");
CheckStrongMode("print(++undefined);");
CheckStrongMode("let x = ++undefined;");

// Prefix decrement with undefined
CheckStrongMode("--undefined;");
CheckStrongMode("print(--undefined);");
CheckStrongMode("let x = --undefined;");

// Function constructor: 'undefined' parameter name
assertDoesNotThrow(function() {
  Function("undefined", "");
});
assertThrows(function() {
  Function("undefined", "'use strong';");
}, SyntaxError);

// Arrow functions with undefined parameters
CheckStrongMode("(undefined => {return});");
assertThrows("(undefined => {'use strong';});");

CheckStrongMode("((undefined, b, c) => {return});");
assertThrows("((undefined, b, c) => {'use strong';});");

CheckStrongMode("((a, undefined, c) => {return});");
assertThrows("((a, undefined, c) => {'use strong';});");

CheckStrongMode("((a, b, undefined) => {return});");
assertThrows("((a, b, undefined) => {'use strong';});");
