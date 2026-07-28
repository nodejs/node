// Copyright 2017 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax

function MODULE() {
  "use asm";
  function f() {
    bogus_function_table[0 & LIMIT]();
  }
  return { f:f };
}

var bogus_function_table = [ Object ];
var test_set = [ 0x3fffffff, 0x7fffffff, 0xffffffff ];
for (var i = 0; i < test_set.length; ++i) {
  bogus_function_table[i] = Object;
  var src = MODULE.toString();
  src = src.replace(/MODULE/g, "Module" + i);
  src = src.replace(/LIMIT/g, test_set[i]);
  var module = eval("(" + src + ")");
  module(this).f();
  assertFalse(%IsAsmWasmCode(module));
}
