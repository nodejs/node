// Copyright 2017 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

load('test/mjsunit/wasm/wasm-module-builder.js');

(function() {
  var builder = new WasmModuleBuilder();
  builder.addMemory(31, 31, false);
  builder.addFunction('test', kSig_l_v)
      .addBodyWithEnd([
// body:
kExprUnreachable,
kExprEnd,   // @374
      ])
      .exportFunc();
  var module = builder.instantiate();
})();
