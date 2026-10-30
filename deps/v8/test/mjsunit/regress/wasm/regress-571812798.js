// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --wasm-js-interop

d8.file.execute("test/mjsunit/wasm/wasm-module-builder.js");
d8.file.execute("test/mjsunit/wasm/prototype-setup-builder.js");

let builder = new WasmModuleBuilder();
let proto_config = new WasmPrototypeSetupBuilder(builder);

let $proto = builder.addImportedGlobal("p", "proto", kWasmExternRef);

let ctor = builder.addFunction("ctor", kSig_v_v).addBody([]);
let method = builder.addFunction("method", kSig_v_v).addBody([]);

proto_config.addConfig($proto)
            .addConstructor("C", ctor)
            .addMethod("m", kWasmMethod, method);

proto_config.build();

let proto = {};
let constructors = new Proxy({}, {
  defineProperty(target, prop, desc) {
    Object.defineProperty(proto, "m", {
      value: "old",
      writable: false,
      configurable: false,
    });
    return Reflect.defineProperty(target, prop, desc);
  }
});

let imports = {
    p: {proto},
    c: {constructors},
};

assertThrows(() => builder.instantiate(imports, {builtins: ["js-prototypes"]}),
             TypeError, "Cannot redefine property: m");
assertEquals(proto.m, "old");
