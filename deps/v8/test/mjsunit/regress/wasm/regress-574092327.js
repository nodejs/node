// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --wasm-js-interop

d8.file.execute('test/mjsunit/wasm/wasm-module-builder.js');
d8.file.execute('test/mjsunit/wasm/prototype-setup-builder.js');

// Test 1: Constructing an instance inside a `constructors` Proxy defineProperty
// trap must see a prototype that already has a prototype map.
(function TestConstructInProxyTrap() {
  let builder = new WasmModuleBuilder();
  let proto_config = new WasmPrototypeSetupBuilder(builder);
  let $proto = builder.addImportedGlobal('p', 'proto', kWasmExternRef);
  let ctor = builder.addFunction('ctor', kSig_v_v).addBody([]);

  proto_config.addConfig($proto).addConstructor('C', ctor);
  proto_config.build();

  function loadMissing(obj) {
    return obj.foo;
  }

  let proto = {};
  let constructors = new Proxy({}, {
    defineProperty(target, prop, desc) {
      let C = desc.value;
      let inst = new C();
      for (let i = 0; i < 20; i++) {
        assertEquals(undefined, loadMissing(inst));
      }
      proto.foo = 1;
      assertEquals(1, loadMissing(inst));
      return Reflect.defineProperty(target, prop, desc);
    }
  });

  builder.instantiate(
      {p: {proto}, c: {constructors}}, {builtins: ['js-prototypes']});
  assertEquals(1, loadMissing(new constructors.C()));
})();

// Test 2: If the `constructors` Proxy trap deletes `proto.constructor` (making
// the prototype eligible for the method-installation fast path again) and warms
// up a LoadIC on an absent method, fast-path method installation must still
// invalidate prototype chains afterwards.
(function TestFastPathInvalidatesPrototypeChainsAfterProxyTrap() {
  let builder = new WasmModuleBuilder();
  let proto_config = new WasmPrototypeSetupBuilder(builder);
  let $proto = builder.addImportedGlobal('p', 'proto', kWasmExternRef);
  let ctor = builder.addFunction('ctor', kSig_v_v).addBody([]);
  let method = builder.addFunction('m', kSig_i_r).addBody([kExprI32Const, 42]);

  proto_config.addConfig($proto).addConstructor('C', ctor).addMethod(
      'm', kWasmMethod, method);
  proto_config.build();

  function callOrUndefined(obj) {
    return obj.m;
  }

  let proto = {};
  let inst;
  let constructors = new Proxy({}, {
    defineProperty(target, prop, desc) {
      delete proto.constructor;
      let C = desc.value;
      inst = new C();
      for (let i = 0; i < 20; i++) {
        assertEquals(undefined, callOrUndefined(inst));
      }
      return Reflect.defineProperty(target, prop, desc);
    }
  });

  builder.instantiate(
      {p: {proto}, c: {constructors}}, {builtins: ['js-prototypes']});
  assertEquals('function', typeof callOrUndefined(inst));
  assertEquals(42, inst.m());
})();

// Test 3: Plain `constructors` object where `configureAll` throws after
// installing the constructor; `new constructors.C()` must still see a prototype
// map.
(function TestConstructAfterThrow() {
  let builder = new WasmModuleBuilder();
  let $array_externref = builder.addArray(kWasmExternRef, {final: true});
  let $array_funcref = builder.addArray(kWasmFuncRef, {final: true});
  let $array_i8 = builder.addArray(kWasmI8, {final: true});
  let $configureAll = builder.addImport(
      'wasm:js-prototypes', 'configureAll',
      makeSig(
          [
            wasmRefNullType($array_externref), wasmRefNullType($array_funcref),
            wasmRefNullType($array_i8), kWasmExternRef
          ],
          []));
  let $constructors =
      builder.addImportedGlobal('c', 'constructors', kWasmExternRef);
  let $proto = builder.addImportedGlobal('p', 'proto', kWasmExternRef);
  let ctor = builder.addFunction('ctor', kSig_v_v).addBody([]);

  // 1 prototype, has_constructor = 1, name = "C", then truncated data so
  // `configureAll` throws WebAssembly.RuntimeError after InstallConstructor.
  let data = [1, 1, 1, 67];
  let proto_seg = builder.addPassiveElementSegment(
      [[kExprGlobalGet, $proto]], kWasmExternRef);
  let func_seg = builder.addPassiveElementSegment([ctor.index]);
  let data_seg = builder.addPassiveDataSegment(data);

  let start = builder.addFunction('start', kSig_v_v).addBody([
    kExprI32Const,     0,
    kExprI32Const,     1,
    kGCPrefix,         kExprArrayNewElem,
    $array_externref,  proto_seg,
    kExprI32Const,     0,
    kExprI32Const,     1,
    kGCPrefix,         kExprArrayNewElem,
    $array_funcref,    func_seg,
    kExprI32Const,     0,
    kExprI32Const,     data.length,
    kGCPrefix,         kExprArrayNewData,
    $array_i8,         data_seg,
    kExprGlobalGet,    $constructors,
    kExprCallFunction, $configureAll,
  ]);
  builder.addStart(start.index);

  function loadMissing(obj) {
    return obj.foo;
  }

  let proto = {};
  let constructors = {};
  assertThrows(
      () => builder.instantiate(
          {p: {proto}, c: {constructors}}, {builtins: ['js-prototypes']}),
      WebAssembly.RuntimeError);
  let inst = new constructors.C();
  for (let i = 0; i < 20; i++) {
    assertEquals(undefined, loadMissing(inst));
  }
  proto.foo = 7;
  assertEquals(7, loadMissing(inst));
})();
