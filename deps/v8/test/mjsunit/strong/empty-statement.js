// Copyright 2015 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --strong-mode

(function NoEmptySubStatement() {
  assertThrows("'use strong'; if (1);", SyntaxError);
  assertThrows("'use strong'; if (1) {} else;", SyntaxError);
  assertThrows("'use strong'; while (1);", SyntaxError);
  assertThrows("'use strong'; do; while (1);", SyntaxError);
  assertThrows("'use strong'; for (;;);", SyntaxError);
  assertThrows("'use strong'; for (x in []);", SyntaxError);
  assertThrows("'use strong'; for (x of []);", SyntaxError);
  assertThrows("'use strong'; for (let x;;);", SyntaxError);
  assertThrows("'use strong'; for (let x in []);", SyntaxError);
  assertThrows("'use strong'; for (let x of []);", SyntaxError);
})();
