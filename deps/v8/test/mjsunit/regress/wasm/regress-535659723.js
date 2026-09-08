// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --expose-fast-api --allow-natives-syntax --turbo-fast-api-calls
// Flags: --wasm-fast-api --no-wasm-trap-handler --stress-wasm-memory-moving

d8.file.execute("test/mjsunit/wasm/wasm-module-builder.js");

const fast_c_api = new d8.test.FastCAPI();
const api_function = fast_c_api.call_to_number;
const bound_func = Function.prototype.call.bind(api_function);

const builder = new WasmModuleBuilder();
builder.addMemory(1, 100);
builder.exportMemoryAs('mem');

const sig_index = builder.addType(makeSig([kWasmExternRef, kWasmExternRef], []));
const $call_to_number = builder.addImport('env', 'call_to_number', sig_index);

builder.addFunction('test', makeSig([kWasmExternRef, kWasmExternRef], []))
    .exportFunc()
    .addBody([
      // Call Fast API call_to_number
      kExprLocalGet, 0, // receiver (fast_c_api)
      kExprLocalGet, 1, // arg
      kExprCallFunction, $call_to_number,

      // Write to memory
      kExprI32Const, 4,
      ...wasmI32Const(0x41414141),
      kExprI32StoreMem, 2, 0,
    ]);

const instance = builder.instantiate({env: {call_to_number: bound_func}});
const mem = instance.exports.mem;

%WasmTierUpFunction(instance.exports.test);

let arg = {
  valueOf: function() {
    print("Growing memory inside Fast API callback...");
    mem.grow(10);
    return 42;
  }
};

print("Calling test...");
instance.exports.test(fast_c_api, arg);
print("Done!");
