// Copyright 2017 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --validate-asm

function Module(stdlib, foreign, heap) {
  "use asm";
  var a = stdlib.Math.PI;
  function f() { return a }
  return { f:f };
}
Module.length
