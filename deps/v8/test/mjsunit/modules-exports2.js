// Copyright 2016 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// MODULE

export function foo() { return 42 }
assertEquals(42, foo());
foo = 1;
assertEquals(1, foo);

let gaga = 43;
export {gaga as gugu};
assertEquals(43, gaga);

export default (function bar() { return 43 })
assertThrows(() => bar(), ReferenceError);
assertThrows("default", SyntaxError);
assertThrows("*default*", SyntaxError);


var bla = 44;
var blu = 45;
export {bla};
export {bla as blu};
export {bla as bli};
assertEquals(44, bla);
assertEquals(45, blu);
bla = 46;
assertEquals(46, bla);
assertEquals(45, blu);
