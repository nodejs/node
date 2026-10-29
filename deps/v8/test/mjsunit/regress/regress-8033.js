// Copyright 2018 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

assertThrows("foo: if (true) do { continue foo } while (false)", SyntaxError);
assertThrows("foo: if (true) while (false) { continue foo }", SyntaxError);
assertThrows("foo: if (true) for (; false; ) { continue foo }", SyntaxError);
assertThrows("foo: if (true) for (let x of []) { continue foo }", SyntaxError);
assertThrows("foo: if (true) for (let x in []) { continue foo }", SyntaxError);

assertThrows("foo: if (true) { do { continue foo } while (false) }", SyntaxError);
assertThrows("foo: if (true) { while (false) { continue foo } }", SyntaxError);
assertThrows("foo: if (true) { for (; false; ) { continue foo } }", SyntaxError);
assertThrows("foo: if (true) { for (let x of []) { continue foo } }", SyntaxError);
assertThrows("foo: if (true) { for (let x in []) { continue foo } }", SyntaxError);

assertThrows("foo: goo: if (true) do { continue foo } while (false)", SyntaxError);
assertThrows("foo: goo: if (true) while (false) { continue foo }", SyntaxError);
assertThrows("foo: goo: if (true) for (; false; ) { continue foo }", SyntaxError);
assertThrows("foo: goo: if (true) for (let x of []) { continue foo }", SyntaxError);
assertThrows("foo: goo: if (true) for (let x in []) { continue foo }", SyntaxError);

assertThrows("foo: goo: if (true) { do { continue foo } while (false) }", SyntaxError);
assertThrows("foo: goo: if (true) { while (false) { continue foo } }", SyntaxError);
assertThrows("foo: goo: if (true) { for (; false; ) { continue foo } }", SyntaxError);
assertThrows("foo: goo: if (true) { for (let x of []) { continue foo } }", SyntaxError);
assertThrows("foo: goo: if (true) { for (let x in []) { continue foo } }", SyntaxError);

assertDoesNotThrow("if (true) foo: goo: do { continue foo } while (false)");
assertDoesNotThrow("if (true) foo: goo: while (false) { continue foo }");
assertDoesNotThrow("if (true) foo: goo: for (; false; ) { continue foo }");
assertDoesNotThrow("if (true) foo: goo: for (let x of []) { continue foo }");
assertDoesNotThrow("if (true) foo: goo: for (let x in []) { continue foo }");

assertThrows("if (true) foo: goo: { do { continue foo } while (false) }", SyntaxError);
assertThrows("if (true) foo: goo: { while (false) { continue foo } }", SyntaxError);
assertThrows("if (true) foo: goo: { for (; false; ) { continue foo } }", SyntaxError);
assertThrows("if (true) foo: goo: { for (let x of []) { continue foo } }", SyntaxError);
assertThrows("if (true) foo: goo: { for (let x in []) { continue foo } }", SyntaxError);

assertDoesNotThrow("if (true) { foo: goo: do { continue foo } while (false) }");
assertDoesNotThrow("if (true) { foo: goo: while (false) { continue foo } }");
assertDoesNotThrow("if (true) { foo: goo: for (; false; ) { continue foo } }");
assertDoesNotThrow("if (true) { foo: goo: for (let x of []) { continue foo } }");
assertDoesNotThrow("if (true) { foo: goo: for (let x in []) { continue foo } }");
