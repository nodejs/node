// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --wasm-stringref --turboshaft-verify-load-elimination

d8.file.execute("test/mjsunit/wasm/wasm-module-builder.js");

const builder = new WasmModuleBuilder();

let $sig_v_v = builder.addType(kSig_v_v);
let $sig4 = builder.addType(makeSig([kWasmI32], [kWasmExternRef]));
let $sig5 = builder.addType(makeSig([kWasmExternRef, kWasmI32], [kWasmI32]));
let import_0_v2 = builder.addImport('imports', 'import_0_v2', $sig4);
let charCodeAt = builder.addImport('wasm:js-string', 'charCodeAt', $sig5);
let $global0 = builder.addImportedGlobal('"', 'foo', wasmRefType(kWasmExternRef));

let $var0 = 0;
let $var1 = 1;
let $var2 = 2;
let $var3 = 3;
let w0 = builder.addFunction("w0", $sig_v_v).exportFunc()
  .addLocals(wasmRefType(kWasmArrayRef), 1)  // $var0
  .addLocals(wasmRefType(kWasmExternRef), 1)  // $var1
  .addLocals(kWasmI32, 1)  // $var2
  .addLocals(wasmRefType(kWasmAnyRef), 1)  // $var3
  .addBody([
    kExprRefNull, kArrayRefCode,
    kExprBrOnNull, 0,
    kExprLocalSet, $var0,
    kExprGlobalGet, $global0,
    kExprLocalSet, $var1,
    kExprLocalGet, $var1,
    kGCPrefix, kExprAnyConvertExtern,
    kGCPrefix, kExprRefTest, kEqRefCode,
    kExprLocalSet, $var2,
    kExprBlock, $sig_v_v,
      kExprBlock, $sig_v_v,
        kExprBlock, $sig_v_v,
          kExprLocalGet, $var2,
          kExprBrTable, 2, 2, 0, 1,
        kExprEnd,
        kExprLocalGet, $var1,
        kExprI32Const, 0,
        kExprCallFunction, charCodeAt,
        kExprLocalSet, $var2,
      kExprEnd,
      kExprTry, kWasmVoid,
        kExprI32Const, 0,
        kExprCallFunction, import_0_v2,
        kExprDrop,
      kExprCatchAll,
        kExprLocalGet, $var1,
        kGCPrefix, kExprAnyConvertExtern,
        kExprLocalSet, $var3,
      kExprEnd,
    kExprEnd,
  ]);

const instance = builder.instantiate(
  { imports: { import_0_v2: Symbol } },
  { builtins: ['js-string'], importedStringConstants: '"' }
);

instance.exports.w0();
