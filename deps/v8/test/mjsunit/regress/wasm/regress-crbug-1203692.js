// Copyright 2021 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

function asm(stdlib, foreign) {
  "use asm";
  var unused = foreign.a | 0;
  function fun() { }
  return fun;
}

assertThrows(() => asm(null, { a: 1n }).fun(), TypeError);
