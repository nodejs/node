// Copyright 2015 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --strong-mode

(function NoForInStatement() {
  assertThrows("'use strong'; for (x in []) {}", SyntaxError);
  assertThrows("'use strong'; for (let x in []) {}", SyntaxError);
  assertThrows("'use strong'; for (const x in []) {}", SyntaxError);
})();

(function ForOfStatement() {
  assertTrue(eval("'use strong'; for (x of []) {} true"));
  assertTrue(eval("'use strong'; for (let x of []) {} true"));
  assertTrue(eval("'use strong'; for (const x of []) {} true"));
})();
