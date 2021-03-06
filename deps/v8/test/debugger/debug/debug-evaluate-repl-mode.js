// Copyright 2019 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

Debug = debug.Debug

const evaluate = Debug.evaluateGlobalREPL;
const evaluateNonREPL = (source) => Debug.evaluateGlobal(source, false).value();

(async () => {

// Declare let and get value
let result;
result = await evaluate("let x = 7;");
result = await evaluate("x;");
assertEquals(7, result);

// Re-declare in the same script after declaration in another script.
assertThrows(() => evaluate("let x = 8; let x = 9;"));
result = await evaluate("x;");
assertEquals(7, result);

// Re-declare let as let
assertDoesNotThrow(async () => result = await evaluate("let x = 8;"));
result = await evaluate("x;");
assertEquals(8, result);

await evaluate("let x = 8;");

// Close over let. Inner function is only pre-parsed.
result = await evaluate("function getter() { return x; }");
assertEquals(undefined, result);
result = await evaluate("getter();");
assertEquals(8, result);
result = await evaluate("x = 9;");
assertEquals(9, result);
result = await evaluate("x;");
assertEquals(9, result);
result = await evaluate("getter();");
assertEquals(9, result);
// Modifies the original x; does not create a new one/shadow.
result = await evaluate("let x = 10;");
assertEquals(undefined, result);
result = await evaluate("x;");
assertEquals(10, result);
result = await evaluate("getter();");
assertEquals(10, result);

await evaluate("let x = 10");

// Check store from an inner scope.
result = await evaluate("{ let z; x = 11; } x;");
assertEquals(11, result);

// Check re-declare from an inner scope does nothing.
result = await evaluate("{ let z; let x = 12; } x;");
assertEquals(11, result);

assertThrowsAsync(evaluate("{ let qq = 10; } qq;"),
    ReferenceError, "qq is not defined");

// Re-declare in the same script (no previous declaration).
assertThrows(() => result = evaluate("let y = 7; let y = 8;"),
    SyntaxError, "Identifier 'y' has already been declared");

// Check TDZ; use before initialization.
// Do not check exact error message, it depends on the path taken through
// the IC machinery and changes sometimes, causing the test to be flaky.
assertThrowsAsync(evaluate("a; let a = 7;"), ReferenceError);
assertThrowsAsync(evaluate("a;"), ReferenceError);
// This is different to non-REPL mode, which throws the kUndefined error here.
assertThrowsAsync(evaluate("a = 7;"),
    ReferenceError, "Cannot access 'a' before initialization");

result = await evaluate("let a = 8;");
assertEquals(undefined, result);
result = await evaluate("a;")
assertEquals(8, result);

// Check TDZ; store before initialization.
assertThrowsAsync(evaluate("b = 10; let b;"),
    ReferenceError, "Cannot access 'b' before initialization");
// Check that b is still broken.
assertThrowsAsync(evaluate("b = 10; let b;"),
    ReferenceError, "Cannot access 'b' before initialization");
// Check that b is still broken when the let defines a value.
assertThrowsAsync(evaluate("b = 10; let b = 7;"),
    ReferenceError, "Cannot access 'b' before initialization");
result = await evaluate("let b = 11;");
assertEquals(undefined, result);
// We fixed 'b'!
result = await evaluate("b;");
assertEquals(11, result);

// Check that class works the same. Internally there is no difference between
// class and let so we don't test as extensively as for let.
result = evaluate("class K {};");
assertDoesNotThrow(() => result = evaluate("class K {};"));

// many tests for normal/repl script interactions.

// tests with let x = await

// result = evaluate("toString;");

// Re-declare let as const
evaluate("let z = 10;");
assertThrows(() => result = evaluate("const z = 9;"),
    SyntaxError, "Identifier 'z' has already been declared");
result = await evaluate("z;");
assertEquals(10, result);

// Re-declare const as const
result = await evaluate("const c = 10;");
assertThrows(() => result = evaluate("const c = 11;"),
    SyntaxError, "Identifier 'c' has already been declared");
result = await evaluate("c;");
assertEquals(10, result);

// Const vs. const in same script.
assertThrows(() => result = evaluate("const d = 9; const d = 10;"),
SyntaxError, "Identifier 'd' has already been declared");

// Close over const
result = await evaluate("const e = 10; function closure() { return e; }");
result = await evaluate("e;");
assertEquals(10, result);

// Assign to const
assertThrowsAsync(evaluate("e = 11;"),
    TypeError, "Assignment to constant variable.");
result = await evaluate("e;");
assertEquals(10, result);
result = await evaluate("closure();");
assertEquals(10, result);

// Assign to const in TDZ
assertThrowsAsync(evaluate("f; const f = 11;"),
    ReferenceError, "Cannot access 'f' before initialization");
assertThrowsAsync(evaluate("f = 12;"),
    TypeError, "Assignment to constant variable.");

// Re-declare const as let
result = await evaluate("const g = 12;");
assertThrows(() => result = evaluate("let g = 13;"),
    SyntaxError, "Identifier 'g' has already been declared");
result = await evaluate("g;");
assertEquals(12, result);

// Let vs. const in the same script
assertThrows(() => result = evaluate("let h = 13; const h = 14;"),
    SyntaxError, "Identifier 'h' has already been declared");
assertThrows(() => result = evaluate("const i = 13; let i = 14;"),
    SyntaxError, "Identifier 'i' has already been declared");

// Configurable properties of the global object can be re-declared as let.
result = await evaluate(`Object.defineProperty(globalThis, 'j', {
  value: 1,
  configurable: true
});`);
result = await evaluate("j;");
assertEquals(1, result);
result = await evaluate("let j = 2;");
result = await evaluate("j;");
assertEquals(2, result);

// Non-configurable properties of the global object (also created by plain old
// top-level var declarations) cannot be re-declared as let.
result = await evaluate(`Object.defineProperty(globalThis, 'k', {
  value: 1,
  configurable: false
});`);
result = await evaluate("k;");
assertEquals(1, result);
assertThrows(() => result = evaluate("let k = 2;"),
    SyntaxError, "Identifier 'k' has already been declared");
result = await evaluate("k;");
assertEquals(1, result);

// ... Except if you do it in the same script.
result = await evaluate(`Object.defineProperty(globalThis, 'k2', {
  value: 1,
  configurable: false
});
let k2 = 2;`);
result = await evaluate("k2;");
assertEquals(2, result);
result = await evaluate("globalThis.k2;");
assertEquals(1, result);

// But if the property is configurable then both versions are allowed.
result = await evaluate(`Object.defineProperty(globalThis, 'k3', {
  value: 1,
  configurable: true
});`);
result = await evaluate("k3;");
assertEquals(1, result);
result = await evaluate("let k3 = 2;");
result = await evaluate("k3;");
assertEquals(2, result);
result = await evaluate("globalThis.k3;");
assertEquals(1, result);

result = await evaluate(`Object.defineProperty(globalThis, 'k4', {
  value: 1,
  configurable: true
});
let k4 = 2;`);
result = await evaluate("k4;");
assertEquals(2, result);
result = await evaluate("globalThis.k4;");
assertEquals(1, result);

// Check var followed by let in the same script.
assertThrows(() => result = evaluate("var k5 = 1; let k5 = 2;"),
SyntaxError, "Identifier 'k5' has already been declared");

// In different scripts.
result = await evaluate("var k6 = 1;");
assertThrows(() => result = evaluate("let k6 = 2;"),
    SyntaxError, "Identifier 'k6' has already been declared");

// Check let followed by var in the same script.
assertThrows(() => result = evaluate("let k7 = 1; var k7 = 2;"),
SyntaxError, "Identifier 'k7' has already been declared");

// In different scripts.
result = evaluate("let k8 = 1;");
assertThrows(() => result = evaluate("var k8 = 2;"),
    SyntaxError, "Identifier 'k8' has already been declared");

// Check var followed by var in the same script.
result = await evaluate("var k9 = 1; var k9 = 2;");
result = await evaluate("k9;");
assertEquals(2, result);

// In different scripts.
result = await evaluate("var k10 = 1;");
result = await evaluate("var k10 = 2;");
result = await evaluate("k10;");
assertEquals(2, result);
result = await evaluate("globalThis.k10;");
assertEquals(2, result);

// typeof should not throw for undeclared variables
result = await evaluate("typeof k11");
assertEquals("undefined", result);

// Test lets with names on the object prototype e.g. toString to make sure
// it only works for own properties.
// result = evaluate("let valueOf;");

// REPL vs. non-REPL scripts

// We can still read let values cross-mode.
result = evaluateNonREPL("let l1 = 1; let l2 = 2; let l3 = 3;");
result = await evaluate("l1;");
assertEquals(1, result);

// But we can't re-declare page script lets in a REPL script. We might want to
// later.
assertThrows(() => result = evaluate("let l1 = 2;"),
    SyntaxError, "Identifier 'l1' has already been declared");

assertThrows(() => result = evaluate("var l2 = 3;"),
    SyntaxError, "Identifier 'l2' has already been declared");

assertThrows(() => result = evaluate("const l3 = 4;"),
    SyntaxError, "Identifier 'l3' has already been declared");

// Re-declaring var across modes works.
result = evaluateNonREPL("var l4 = 1; const l5 = 1;");
result = await evaluate("var l4 = 2;");
result = await evaluate("l4;");
assertEquals(2, result);

// Const doesn't.
assertThrows(() => result = evaluate("const l5 = 2;"),
    SyntaxError, "Identifier 'l5' has already been declared") ;
result = await evaluate("l5;");
assertEquals(1, result);

// Now REPL followed by non-REPL
result = await evaluate("let l6 = 1; const l7 = 2; let l8 = 3;");
result = evaluateNonREPL("l7;");
assertEquals(2, result);
result = evaluateNonREPL("l6;");
assertEquals(1, result);

// Check that the pattern of `l9; let l9;` does not throw for REPL scripts.
// If REPL scripts behaved the same as normal scripts, this case would
// re-introduce the hole in 'l9's script context slot, causing the IC and feedback
// to 'lie' about the current state.
result = await evaluate("let l9;");
result = await evaluate("l9; let l9;"),
assertEquals(undefined, await evaluate('l9;'));

// Check that binding and re-declaring a function via let works.
result = evaluate("let fn1 = function() { return 21; }");
assertEquals(21, fn1());
result = evaluate("let fn1 = function() { return 42; }");
assertEquals(42, fn1());

// Check that lazily parsed functions that bind a REPL-let variable work.
evaluate("let l10 = 21;");
evaluate("let l10 = 42; function fn2() { return l10; }");
evaluate("let l10 = 'foo';");
assertEquals("foo", fn2());

})().catch(e => {
    print(e);
    print(e.stack);
    %AbortJS("Async test is failing");
});
