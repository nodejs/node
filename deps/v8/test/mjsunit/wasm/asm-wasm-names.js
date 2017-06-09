// Copyright 2016 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --validate-asm

function Module(stdlib, foreign, buffer) {
  "use asm";
  function foo() {}
  return {bar: foo};
}

var func = Module({}, {}, new ArrayBuffer(65536)).bar;
assertEquals("Module", Module.name);
assertEquals("foo", func.name);
