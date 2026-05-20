// Copyright 2021 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

var asm = function(global) {
  'use asm';
  function f() {}
  return f;
};
function asm2(global, imports) {
  'use asm';
  var asm = imports.asm;
  function f() {}
  return {f: f};
}
asm2(this, {asm: asm});
