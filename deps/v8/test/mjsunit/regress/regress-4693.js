// Copyright 2016 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//

// In sloppy mode we allow function redeclarations within blocks for webcompat.
(function() {
  assertEquals(undefined, f);  // Annex B
  if (true) {
    assertEquals(2, f());
    function f() { return 1 }
    assertEquals(2, f());
    function f() { return 2 }
    assertEquals(2, f());
  }
  assertEquals(2, f());  // Annex B
})();

// Should still fail in strict mode
assertThrows(`
  (function() {
    "use strict";
    if (true) {
      function f() { return 1 }
      function f() { return 2 }
    }
  })();
`, SyntaxError);

// Conflicts between let and function still throw
assertThrows(`
  (function() {
    if (true) {
      let f;
      function f() { return 2 }
    }
  })();
`, SyntaxError);

assertThrows(`
  (function() {
    if (true) {
      function f() { return 2 }
      let f;
    }
  })();
`, SyntaxError);

// Conflicts between const and function still throw
assertThrows(`
  (function() {
    if (true) {
      const f;
      function f() { return 2 }
    }
  })();
`, SyntaxError);

assertThrows(`
  (function() {
    if (true) {
      function f() { return 2 }
      const f;
    }
  })();
`, SyntaxError);

// Annex B redefinition semantics still apply with more blocks
(function() {
  assertEquals(undefined, f);  // Annex B
  if (true) {
    assertEquals(undefined, f);
    { function f() { return 1 } }
    assertEquals(1, f());
    { function f() { return 2 } }
    assertEquals(2, f());
  }
  assertEquals(2, f());  // Annex B
})();
