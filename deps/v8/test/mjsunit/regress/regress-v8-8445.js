// Copyright 2018 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax

class MyRegExp {
  exec() { return null; }
}

assertEquals(["ab", ""], "abc".split(/c/g));
assertEquals([["a"]], [..."a".matchAll(/a/g)]);

Object.defineProperty(RegExp, Symbol.species, { get() { return MyRegExp; }});

assertEquals(["abc"], "abc".split(/c/g));
assertEquals([], [..."a".matchAll(/a/g)]);
