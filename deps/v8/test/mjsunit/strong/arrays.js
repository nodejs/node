// Copyright 2015 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --strong-mode

(function NoEllisions() {
  assertThrows("'use strong'; [,]", SyntaxError);
  assertThrows("'use strong'; [,3]", SyntaxError);
  assertThrows("'use strong'; [3,,4]", SyntaxError);
  assertTrue(eval("'use strong'; [3,] !== [3,4,]"));
})();
