// Copyright 2015 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --strong-mode

'use strong';

(function NoArguments() {
  assertThrows("'use strong'; arguments", SyntaxError);
  assertThrows("'use strong'; function f() { arguments }", SyntaxError);
  assertThrows("'use strong'; let f = function() { arguments }", SyntaxError);
  assertThrows("'use strong'; let f = () => arguments", SyntaxError);
  // The following are strict mode errors already.
  assertThrows("'use strong'; let arguments", SyntaxError);
  assertThrows("'use strong'; function f(arguments) {}", SyntaxError);
  assertThrows("'use strong'; let f = (arguments) => {}", SyntaxError);
})();

function g() {}

(function LexicalFunctionBindings(global) {
  assertEquals('function', typeof g);
  assertEquals(undefined, global.g);
})(this);

(function ImmutableFunctionBindings() {
  function f() {}
  assertThrows(function(){ eval("g = 0") }, TypeError);
  assertThrows(function(){ eval("f = 0") }, TypeError);
  assertEquals('function', typeof g);
  assertEquals('function', typeof f);
})();
