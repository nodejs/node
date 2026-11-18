// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

d8.file.execute('test/mjsunit/wasm/wasm-module-builder.js');

const builder = new WasmModuleBuilder();
const import_idx = builder.addImport('m', 'f', kSig_v_v);
const start_fn = builder.addFunction('start', kSig_v_v).addBody([
  kExprCallFunction, import_idx,
]);
builder.addStart(start_fn.index);
const module = builder.toModule();

class SubInstance extends WebAssembly.Instance {}

let start_executed = false;
const imports = {
  m: {
    f() {
      start_executed = true;
      Error.prepareStackTrace = (err, stack) => stack;
      // Extract the WasmInstanceObject from the Wasm start function frame
      // while instantiation is still in progress.
      const instance = new Error().stack[1].getThis();
      assertInstanceof(instance, SubInstance);
      Object.preventExtensions(instance);
    }
  }
};

const instance =
    Reflect.construct(WebAssembly.Instance, [module, imports], SubInstance);
assertTrue(start_executed);
assertInstanceof(instance, SubInstance);

(function PreventExtensionsInTableDescriptorGetter() {
  class SubTable extends WebAssembly.Table {}
  const desc = {
    get element() {
      Object.preventExtensions(SubTable.prototype);
      return 'anyfunc';
    },
    initial: 0,
  };
  const table = new SubTable(desc);
  assertInstanceof(table, SubTable);
  assertInstanceof(table, WebAssembly.Table);
})();
