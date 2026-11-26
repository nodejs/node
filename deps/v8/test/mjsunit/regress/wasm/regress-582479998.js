// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --wasm-js-interop

d8.file.execute('test/mjsunit/wasm/wasm-module-builder.js');

const kMethod = 0;
const kGetter = 1;
const kSetter = 2;

function encodeName(name) {
  if (typeof name === 'string') {
    const bytes = [...name].map(c => c.charCodeAt(0));
    return [...wasmUnsignedLeb(bytes.length), ...bytes];
  }
  return [...wasmUnsignedLeb(name.length), ...name];
}

// Builds a module that calls wasm:js-prototypes.configureAll on a single
// prototype with the given method descriptors and funcref entries (each entry
// is either a function name key in `fnMap` or null).
function buildConfigureRunner(methods, funcKinds) {
  const builder = new WasmModuleBuilder();
  const $protos = builder.addArray(kWasmExternRef, {final: true});
  const $funcs = builder.addArray(kWasmFuncRef, {final: true});
  const $data = builder.addArray(kWasmI8, {final: true});
  const $cfg = builder.addType(makeSig(
      [
        wasmRefNullType($protos), wasmRefNullType($funcs),
        wasmRefNullType($data), kWasmExternRef
      ],
      []));
  const $configureAll =
      builder.addImport('wasm:js-prototypes', 'configureAll', $cfg);

  const $g = builder.addGlobal(kWasmI32, true, false);
  const $fn1 = builder.addFunction('fn1', makeSig([kWasmExternRef], [kWasmI32]))
                   .addBody([kExprI32Const, 1]);
  const $fn2 = builder.addFunction('fn2', makeSig([kWasmExternRef], [kWasmI32]))
                   .addBody([kExprI32Const, 2]);
  const $get = builder.addFunction('get', makeSig([kWasmExternRef], [kWasmI32]))
                   .addBody([kExprGlobalGet, $g.index]);
  const $set = builder.addFunction('set', makeSig([kWasmExternRef, kWasmI32], []))
                   .addBody([kExprLocalGet, 1, kExprGlobalSet, $g.index]);
  builder.addDeclarativeElementSegment(
      [$fn1.index, $fn2.index, $get.index, $set.index]);
  const fnMap = {fn1: $fn1, fn2: $fn2, get: $get, set: $set};

  // 1 prototype, parentidx = -1, has_constructor = 0, num_methods.
  const data = [1, 0x7f, 0, ...wasmUnsignedLeb(methods.length)];
  for (const [kind, name] of methods) {
    data.push(kind, ...encodeName(name));
  }
  const dataSeg = builder.addPassiveDataSegment(data);

  const funcPush = [];
  for (const f of funcKinds) {
    if (f === null) {
      funcPush.push(kExprRefNull, kFuncRefCode);
    } else {
      funcPush.push(kExprRefFunc, fnMap[f].index);
    }
  }

  builder.addFunction('run', makeSig([kWasmExternRef], []))
      .addBody([
        kExprLocalGet, 0,
        kGCPrefix, kExprArrayNewFixed, $protos, 1,
        ...funcPush,
        kGCPrefix, kExprArrayNewFixed, $funcs, ...wasmUnsignedLeb(funcKinds.length),
        kExprI32Const, 0,
        ...wasmI32Const(data.length),
        kGCPrefix, kExprArrayNewData, $data, dataSeg,
        kExprRefNull, kExternRefCode,
        kExprCallFunction, $configureAll,
      ])
      .exportFunc();

  return builder.instantiate({}, {builtins: ['js-prototypes']}).exports.run;
}

// 1. Buffered getter must not be lost when a subsequent method traps on a null
// function (comparing fast path on empty proto vs slow path on non-empty proto).
(function TestGetterPreservedOnNullFunctionTrap() {
  const run = buildConfigureRunner(
      [[kGetter, 'a'], [kMethod, 'b']], ['fn1', null]);

  for (const proto of [{}, Object.create(null), {x: 1}]) {
    assertThrows(() => run(proto), WebAssembly.RuntimeError, /null function/);
    assertTrue(Object.hasOwn(proto, 'a'));
    assertFalse(Object.hasOwn(proto, 'b'));
    const desc = Object.getOwnPropertyDescriptor(proto, 'a');
    assertEquals('function', typeof desc.get);
    assertEquals(undefined, desc.set);
    assertFalse(desc.enumerable);
    assertTrue(desc.configurable);
    assertEquals(1, proto.a);
  }
})();

// 2. Buffered getter must not be lost when a subsequent setter for the same
// property traps on a null function.
(function TestSameNameGetterPreservedOnNullSetterTrap() {
  const run = buildConfigureRunner(
      [[kGetter, 'a'], [kSetter, 'a']], ['fn1', null]);

  for (const proto of [{}, {x: 1}]) {
    assertThrows(() => run(proto), WebAssembly.RuntimeError, /null function/);
    assertTrue(Object.hasOwn(proto, 'a'));
    const desc = Object.getOwnPropertyDescriptor(proto, 'a');
    assertEquals('function', typeof desc.get);
    assertEquals(undefined, desc.set);
    assertEquals(1, proto.a);
  }
})();

// 3. Buffered setter must not be lost when a subsequent method traps on an
// out-of-bounds functions array access.
(function TestSetterPreservedOnOobFunctionTrap() {
  const run = buildConfigureRunner(
      [[kSetter, 'a'], [kMethod, 'b']], ['set']);

  for (const proto of [{}, {x: 1}]) {
    assertThrows(
        () => run(proto), WebAssembly.RuntimeError,
        /array element access out of bounds/);
    assertTrue(Object.hasOwn(proto, 'a'));
    const desc = Object.getOwnPropertyDescriptor(proto, 'a');
    assertEquals(undefined, desc.get);
    assertEquals('function', typeof desc.set);
  }
})();

// 4. Invalid UTF-8 in a getter/setter name must trap immediately, before
// reading a subsequent method's function (even if that function is null or
// has the same invalid UTF-8 name).
(function TestInvalidUtf8AccessorValidatedEagerly() {
  const badUtf8 = [0xdf, 0x00];
  const runDiffName = buildConfigureRunner(
      [[kGetter, badUtf8], [kMethod, 'b']], ['fn1', null]);
  const runSameName = buildConfigureRunner(
      [[kSetter, badUtf8], [kGetter, badUtf8]], ['set', null]);

  for (const run of [runDiffName, runSameName]) {
    for (const proto of [{}, {x: 1}]) {
      assertThrows(
          () => run(proto), WebAssembly.RuntimeError, /invalid UTF-8 string/);
    }
  }
})();

// 5. Properties installed before dictionary growth (both data methods and
// accessors) must not be lost when a later method traps (null function or
// invalid UTF-8).
(function TestDictionaryGrowthPreservedOnError() {
  const kCount = 20;
  for (const kind of [kMethod, kGetter]) {
    const methodsNull = [];
    const funcsNull = [];
    for (let i = 0; i < kCount; i++) {
      methodsNull.push([kind, 'm' + i]);
      funcsNull.push('fn1');
    }
    methodsNull.push([kind, 'trap']);
    funcsNull.push(null);

    const runNull = buildConfigureRunner(methodsNull, funcsNull);
    for (const proto of [{}, Object.create(null), {x: 1}]) {
      const wm = new WeakMap([[proto, 'hash']]);
      assertThrows(
          () => runNull(proto), WebAssembly.RuntimeError, /null function/);
      for (let i = 0; i < kCount; i++) {
        assertTrue(Object.hasOwn(proto, 'm' + i));
      }
      assertFalse(Object.hasOwn(proto, 'trap'));
      assertEquals('hash', wm.get(proto));
    }

    const methodsBadUtf8 = methodsNull.slice(0, kCount);
    methodsBadUtf8.push([kind, [0xdf, 0x00]]);
    const funcsBadUtf8 = funcsNull.slice(0, kCount);
    funcsBadUtf8.push('fn1');

    const runBadUtf8 = buildConfigureRunner(methodsBadUtf8, funcsBadUtf8);
    for (const proto of [{}, {x: 1}]) {
      assertThrows(
          () => runBadUtf8(proto), WebAssembly.RuntimeError,
          /invalid UTF-8 string/);
      for (let i = 0; i < kCount; i++) {
        assertTrue(Object.hasOwn(proto, 'm' + i));
      }
    }
  }
})();

// 6. Accessor/method combinations on the fast path match the slow path:
// consecutive getter+setter, duplicate getter, getter->method->setter
// overwrite, non-consecutive getter/setter (both directions and with cached
// last_accessor_pair updates), array-index bailout, and empty name.
(function TestFastPathAccessorEdgeCases() {
  const run = buildConfigureRunner(
      [
        [kGetter, 'pair'],
        [kSetter, 'pair'],
        [kGetter, 'dup'],
        [kGetter, 'dup'],
        [kGetter, 'overwrite'],
        [kMethod, 'overwrite'],
        [kSetter, 'overwrite'],
        [kMethod, 'ow2'],
        [kGetter, 'other1'],
        [kGetter, 'ow2'],
        [kSetter, 'ow2'],
        [kGetter, 'split'],
        [kMethod, 'mid'],
        [kSetter, 'split'],
        [kSetter, 'split2'],
        [kGetter, 'other2'],
        [kGetter, 'split2'],
        [kSetter, 'split2'],
        [kGetter, ''],
        [kSetter, ''],
        [kGetter, 'beforeBailout'],
        [kMethod, '0'],
        [kSetter, 'beforeBailout'],
      ],
      [
        'get', 'set', 'fn1', 'fn2', 'fn1', 'fn2', 'set', 'fn1', 'fn1', 'get',
        'set', 'get', 'fn1', 'set', 'set', 'fn2', 'get', 'set', 'get', 'set',
        'get', 'fn1', 'set',
      ]);

  const fastProto = {};
  const slowProto = {x: 1};
  run(fastProto);
  run(slowProto);

  const expectedNames = [
    '0', 'pair', 'dup', 'overwrite', 'ow2', 'other1', 'split', 'mid', 'split2',
    'other2', '', 'beforeBailout',
  ];
  assertEquals(expectedNames, Object.getOwnPropertyNames(fastProto));
  assertEquals(
      ['0', 'x', ...expectedNames.slice(1)],
      Object.getOwnPropertyNames(slowProto));

  for (const proto of [fastProto, slowProto]) {
    proto.pair = 11;
    assertEquals(11, proto.pair);

    assertEquals(2, proto.dup);

    const overwriteDesc = Object.getOwnPropertyDescriptor(proto, 'overwrite');
    assertEquals(undefined, overwriteDesc.get);
    assertEquals('function', typeof overwriteDesc.set);

    // Verify ow2 updated last_accessor_pair instead of mutating other1.
    const other1Desc = Object.getOwnPropertyDescriptor(proto, 'other1');
    assertEquals(1, proto.other1);
    assertEquals(undefined, other1Desc.set);
    proto.ow2 = 15;
    assertEquals(15, proto.ow2);

    proto.split = 22;
    assertEquals(22, proto.split);

    // Verify split2 updated last_accessor_pair instead of mutating other2.
    const other2Desc = Object.getOwnPropertyDescriptor(proto, 'other2');
    assertEquals(2, proto.other2);
    assertEquals(undefined, other2Desc.set);
    proto.split2 = 25;
    assertEquals(25, proto.split2);

    proto[''] = 33;
    assertEquals(33, proto['']);

    proto.beforeBailout = 44;
    assertEquals(44, proto.beforeBailout);

    assertEquals(1, proto[0]());
    assertEquals(1, proto.mid());
  }
})();
