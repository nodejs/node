// Copyright 2016 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// MODULE

export { myvar, mylet, myconst };

var myvar = "VAR";
assertEquals("VAR", myvar);
let mylet = "LET";
assertEquals("LET", mylet);
const myconst = "CONST";
assertEquals("CONST", myconst);

function* gaga() { yield 1 }
assertEquals(1, gaga().next().value);
export {gaga};
export default gaga;
export {gaga as gigi};
assertEquals(1, gaga().next().value);


export let gugu = 42;

{
  assertEquals(42, gugu);
}

try {
  assertEquals(42, gugu);
} catch(_) {
  assertUnreachable();
}

try {
  throw {};
} catch(_) {
  assertEquals(42, gugu);
}

try {
  throw {};
} catch({x=gugu}) {
  assertEquals(42, x);
}

assertEquals(5, eval("var x = 5; x"));
