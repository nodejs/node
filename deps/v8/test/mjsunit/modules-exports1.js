// Copyright 2016 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// MODULE

export var myvar = "VAR";
assertEquals("VAR", myvar);
assertEquals("VAR", eval("myvar"));
(() => assertEquals("VAR", myvar))();

export let mylet = "LET";
assertEquals("LET", mylet);
assertEquals("LET", eval("mylet"));
(() => assertEquals("LET", mylet))();

export const myconst = "CONST";
assertEquals("CONST", myconst);
assertEquals("CONST", eval("myconst"));
(() => assertEquals("CONST", myconst))();


myvar = 1;
assertEquals(1, myvar);
assertEquals(1, eval("myvar"));
(() => assertEquals(1, myvar))();
(() => myvar = 2)();
assertEquals(2, myvar);
(() => assertEquals(2, myvar))();
{
  let f = () => assertEquals(2, myvar);
  f();
}

mylet = 1;
assertEquals(1, mylet);
assertEquals(1, eval("mylet"));
(() => assertEquals(1, mylet))();
(() => mylet = 2)();
assertEquals(2, mylet);
assertEquals(2, eval("mylet"));
(() => assertEquals(2, mylet))();
{
  let f = () => assertEquals(2, mylet);
  f();
}

assertThrows(() => myconst = 1, TypeError);
assertEquals("CONST", myconst);
assertEquals("CONST", eval("myconst"));
(() => assertEquals("CONST", myconst))();
{
  let f = () => assertEquals("CONST", myconst);
  f();
}
