// Copyright 2023 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --expose-wasm --allow-natives-syntax --wasm-lazy-compilation

d8.file.execute("test/mjsunit/wasm/wasm-module-builder.js");

const builder = new WasmModuleBuilder();
builder.addFunction('f1', kSig_i_i).addBody([kExprLocalGet, 0]).exportFunc();
builder.addFunction('f2', kSig_i_i).addBody([kExprLocalGet, 0]).exportFunc();
builder.addFunction('f3', kSig_i_i).addBody([kExprLocalGet, 0]).exportFunc();

const instance = builder.instantiate();

instance.exports.f1(1);
instance.exports.f2(2);
instance.exports.f3(3);

assertTrue(%IsLiftoffFunction(instance.exports.f1));
assertTrue(%IsLiftoffFunction(instance.exports.f2));
assertTrue(%IsLiftoffFunction(instance.exports.f3));

%FlushWasmCode();

assertTrue(%IsUncompiledWasmFunction(instance.exports.f1));
assertTrue(%IsUncompiledWasmFunction(instance.exports.f2));
assertTrue(%IsUncompiledWasmFunction(instance.exports.f3));

instance.exports.f1(1);
instance.exports.f2(2);
instance.exports.f3(3);

%WasmTierUpFunction(instance.exports.f3);

%FlushWasmCode();

assertTrue(%IsUncompiledWasmFunction(instance.exports.f1));
assertTrue(%IsUncompiledWasmFunction(instance.exports.f2));
assertTrue(%IsTurboFanFunction(instance.exports.f3));
