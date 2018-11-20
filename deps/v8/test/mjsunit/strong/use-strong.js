// Copyright 2015 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --strong-mode

(function UseStrongScoping() {
  assertThrows("'use strong'; 0 == 0", SyntaxError);
  assertThrows("'use strong'; try {} catch(e) { { 0 == 0 } }", SyntaxError);
  assertThrows("function f() { 'use strong'; 0 == 0 }", SyntaxError);
  assertThrows("'use strong'; function f() { 0 == 0 }", SyntaxError);
  assertThrows("'use strong'; function f() { function g() { 0 == 0 } }", SyntaxError);
  assertThrows("'use strong'; eval('function f() { 0 == 0 }')", SyntaxError);
  assertTrue(eval("function f() { 'use strong' } 0 == 0"));
  assertTrue(eval("eval('\\\'use strong\\\''); 0 == 0"));
})();

(function UseStrongMixed() {
  assertThrows("'use strict'; 'use strong'; 0 == 0", SyntaxError);
  assertThrows("'use strong'; 'use strict'; 0 == 0", SyntaxError);
  assertThrows("'use strong'; 'use strong'; 0 == 0", SyntaxError);
  assertThrows("'use strict'; function f() { 'use strong'; 0 == 0 }", SyntaxError);
  assertThrows("'use strong'; function f() { 'use strict'; 0 == 0 }", SyntaxError);
  assertTrue(eval("'use strict'; function f() { 'use strong' } 0 == 0"));
  assertTrue(eval("var x; function f() { 'use strong' } delete x"));
  assertThrows("'use strict'; var x; function f() { 'use strong' } delete x", SyntaxError);
})();
