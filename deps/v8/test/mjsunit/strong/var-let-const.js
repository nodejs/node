// Copyright 2014 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --strong-mode

(function NoVar() {
  assertThrows("'use strong'; var x = 0;", SyntaxError);
  assertThrows("'use strong'; for(var i = 0; i < 10; ++i) { };", SyntaxError);
})();


(function LetIsOkay() {
  assertTrue(eval("'use strong'; let x = 0; x === 0;"));
  assertTrue(eval("'use strong'; for(let i = 0; i < 10; ++i) { } 0 === 0;"));
})();


(function ConstIsOkay() {
  assertTrue(eval("'use strong'; const x = 0; x === 0;"));
  assertTrue(eval("'use strong'; for(const i = 0; false;) { } 0 === 0;"));
})();
