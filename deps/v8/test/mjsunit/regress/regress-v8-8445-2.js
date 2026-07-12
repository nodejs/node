// Copyright 2018 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax

class MyRegExp {
  exec() { return null; }
}

var r = /c/g;

assertEquals(["ab", ""], "abc".split(r));
assertEquals([["c"]], [..."c".matchAll(r)]);

r.constructor =  { [Symbol.species] : MyRegExp };

assertEquals(["abc"], "abc".split(r));
assertEquals([], [..."c".matchAll(r)]);

assertEquals(["ab", ""], "abc".split(/c/g));
assertEquals([["c"]], [..."c".matchAll(/c/g)]);

RegExp.prototype.constructor =  { [Symbol.species] : MyRegExp };

assertEquals(["abc"], "abc".split(/c/g));
assertEquals([], [..."c".matchAll(/c/g)]);
