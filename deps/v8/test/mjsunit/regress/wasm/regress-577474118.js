// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

d8.file.execute('test/mjsunit/wasm/wasm-module-builder.js');

const builder = new WasmModuleBuilder();
const fn = builder.addImport('mod', 'js', kSig_v_v);
builder.addFunction('á', kSig_v_v)
    .addBody([kExprCallFunction, fn])
    .exportAs('main');

const instance = builder.instantiate({
  mod: {
    js() {
      let p = Promise.reject(123);
      p.catch(() => {});
    }
  }
});
instance.exports.main();
