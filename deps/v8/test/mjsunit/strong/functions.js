// Copyright 2015 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --strong-mode

'use strong';

function f() {}
function* g() {}

(function NoArguments() {
  assertThrows("'use strong'; arguments", SyntaxError);
  assertThrows("'use strong'; function f() { arguments }", SyntaxError);
  assertThrows("'use strong'; function* g() { arguments }", SyntaxError);
  assertThrows("'use strong'; let f = function() { arguments }", SyntaxError);
  assertThrows("'use strong'; let g = function*() { arguments }", SyntaxError);
  assertThrows("'use strong'; let f = () => arguments", SyntaxError);
  // The following are strict mode errors already.
  assertThrows("'use strong'; let arguments", SyntaxError);
  assertThrows("'use strong'; function f(arguments) {}", SyntaxError);
  assertThrows("'use strong'; function* g(arguments) {}", SyntaxError);
  assertThrows("'use strong'; let f = (arguments) => {}", SyntaxError);
})();

(function NoArgumentsProperty() {
  assertFalse(f.hasOwnProperty("arguments"));
  assertFalse(g.hasOwnProperty("arguments"));
  assertThrows(function(){ f.arguments = 0 }, TypeError);
  assertThrows(function(){ g.arguments = 0 }, TypeError);
})();

(function NoCaller() {
  assertFalse(f.hasOwnProperty("caller"));
  assertFalse(g.hasOwnProperty("caller"));
  assertThrows(function(){ f.caller = 0 }, TypeError);
  assertThrows(function(){ g.caller = 0 }, TypeError);
})();

(function NoCallee() {
  assertFalse("callee" in f);
  assertFalse("callee" in g);
  assertThrows(function(){ f.callee = 0 }, TypeError);
  assertThrows(function(){ g.callee = 0 }, TypeError);
})();

(function LexicalBindings(global) {
  assertEquals('function', typeof f);
  assertEquals('function', typeof g);
  assertEquals(undefined, global.f);
  assertEquals(undefined, global.g);
})(this);

(function ImmutableBindings() {
  function f2() {}
  function* g2() {}
  assertThrows(function(){ f = 0 }, TypeError);
  assertThrows(function(){ g = 0 }, TypeError);
  assertThrows(function(){ f2 = 0 }, TypeError);
  assertThrows(function(){ g2 = 0 }, TypeError);
  assertEquals('function', typeof f);
  assertEquals('function', typeof g);
  assertEquals('function', typeof f2);
  assertEquals('function', typeof g2);
})();

(function NonExtensible() {
  assertThrows(function(){ f.a = 0 }, TypeError);
  assertThrows(function(){ g.a = 0 }, TypeError);
  assertThrows(function(){ Object.defineProperty(f, "a", {value: 0}) }, TypeError);
  assertThrows(function(){ Object.defineProperty(g, "a", {value: 0}) }, TypeError);
  assertThrows(function(){ Object.setPrototypeOf(f, {}) }, TypeError);
  assertThrows(function(){ Object.setPrototypeOf(g, {}) }, TypeError);
})();

(function NoPrototype() {
  assertFalse("prototype" in f);
  assertFalse(g.hasOwnProperty("prototype"));
  assertThrows(function(){ f.prototype = 0 }, TypeError);
  assertThrows(function(){ g.prototype = 0 }, TypeError);
  assertThrows(function(){ f.prototype.a = 0 }, TypeError);
})();

(function NonConstructor() {
  assertThrows(function(){ new f }, TypeError);
  assertThrows(function(){ new g }, TypeError);
})();
