// Copyright 2023 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

function asm(stdlib, foreign) {
  'use asm';
  var foreign = +foreign.bar;
  var baz = +foreign.baz;
  function f() {
    return baz;
  }
  return {f: f};
}
const module = asm(this, {baz: 345.7});
assertEquals(NaN, module.f());
