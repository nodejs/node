// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --trace-wasm-typer --trace-turbo-graph --no-liftoff
// Flags: --wasm-lazy-compilation --experimental-wasm-shared

d8.file.execute('test/mjsunit/wasm/wasm-module-builder.js');
d8.file.execute('test/mjsunit/mjsunit.js');

let builder = new WasmModuleBuilder();
builder.addFunction('anyConvertExtern',
  makeSig([kWasmExternRef], [kWasmAnyRef]))
.addBody([
  kExprLocalGet, 0,
  kExprRefAsNonNull,
  kGCPrefix, kExprAnyConvertExtern,
])
.exportFunc();

builder.addFunction('externConvertAny',
  makeSig([kWasmAnyRef], [kWasmExternRef]))
.addBody([
  kExprLocalGet, 0,
  kExprRefAsNonNull,
  kGCPrefix, kExprExternConvertAny,
])
.exportFunc();

let kRefSharedExtern = wasmRefType(kWasmExternRef).shared();
let kRefSharedAny = wasmRefType(kWasmAnyRef).shared();
let kRefNullSharedExtern = wasmRefNullType(kWasmExternRef).shared();
let kRefNullSharedAny = wasmRefNullType(kWasmAnyRef).shared();

builder.addFunction('anyConvertExternShared', makeSig([kRefNullSharedExtern], [kRefNullSharedAny]))
.addBody([
  kExprLocalGet, 0,
  kExprRefAsNonNull,
  kGCPrefix, kExprAnyConvertExtern,
])
.exportFunc();

builder.addFunction('externConvertAnyShared',
  makeSig([kRefNullSharedAny], [kRefSharedExtern]))
.addBody([
  kExprLocalGet, 0,
  kExprRefAsNonNull,
  kGCPrefix, kExprExternConvertAny,
])
.exportFunc();

let instance = builder.instantiate();

// CHECK-LABEL: test any.convert_extern
// CHECK: Refine type for object #{{[0-9]+}}(AnyConvertExtern) -> (ref any)
// CHECK: AnyConvertExtern{{.*}}non-shared, non-nullable
console.log("test any.convert_extern");
assertEquals(123, instance.exports.anyConvertExtern(123));

// CHECK-LABEL: test extern.convert_any
// CHECK: Refine type for object #{{[0-9]+}}(ExternConvertAny) -> (ref extern)
// CHECK: ExternConvertAny{{.*}}non-nullable
console.log("test extern.convert_any");
assertEquals(123, instance.exports.externConvertAny(123));

// CHECK-LABEL: test any.convert_extern shared
// CHECK: Refine type for object #{{[0-9]+}}(AnyConvertExtern) -> (ref shared any)
// CHECK: AnyConvertExtern{{.*}}shared, non-nullable
console.log("test any.convert_extern shared");
assertEquals(123, instance.exports.anyConvertExternShared(123));

// CHECK-LABEL: test extern.convert_any shared
// CHECK: Refine type for object #{{[0-9]+}}(ExternConvertAny) -> (ref shared extern)
// CHECK: ExternConvertAny{{.*}}non-nullable
console.log("test extern.convert_any shared");
assertEquals(123, instance.exports.externConvertAnyShared(123));
