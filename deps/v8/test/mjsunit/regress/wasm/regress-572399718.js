// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax --wasm-custom-descriptors --wasm-js-interop

d8.file.execute('test/mjsunit/wasm/wasm-module-builder.js');
d8.file.execute('test/mjsunit/wasm/prototype-setup-builder.js');

function makeModule(hook) {
  let builder = new WasmModuleBuilder();
  builder.startRecGroup();
  let $obj0 = builder.addStruct({fields: [], descriptor: 1});
  let $desc0 = builder.addStruct(
      {fields: [makeField(kWasmExternRef, false)], describes: $obj0});
  builder.endRecGroup();

  let proto_config = new WasmPrototypeSetupBuilder(builder);
  let kHook = builder.addImport('env', 'hook', kSig_i_v);
  let proto_global = builder.addImportedGlobal('p', 'p0', kWasmExternRef);
  let $make_t = builder.addType(makeSig([], [kWasmAnyRef]));
  let global = builder.addGlobal(wasmRefType($desc0).exact(), false, false, [
    kExprGlobalGet, proto_global, kGCPrefix, kExprStructNew, $desc0,
  ]);
  let ctor = builder.addFunction('make', $make_t).addBody([
    kExprGlobalGet, global.index, kGCPrefix, kExprStructNewDesc, $obj0,
  ]);

  let eie = [kWasmExternRef, kWasmI32, kWasmExternRef];
  builder.addFunction('geti', makeSig(eie, [kWasmI32])).exportFunc().addBody([
    kExprCallFunction, kHook,
  ]);
  let eee = [kWasmExternRef, kWasmExternRef, kWasmExternRef];
  builder.addFunction('getp', makeSig(eee, [kWasmI32])).exportFunc().addBody([
    kExprCallFunction, kHook,
  ]);
  let eiee = [kWasmExternRef, kWasmI32, kWasmExternRef, kWasmExternRef];
  builder.addFunction('seti', makeSig(eiee, [kWasmI32])).exportFunc().addBody([
    kExprCallFunction, kHook,
  ]);
  let eeee = [kWasmExternRef, kWasmExternRef, kWasmExternRef, kWasmExternRef];
  builder.addFunction('setp', makeSig(eeee, [kWasmI32])).exportFunc().addBody([
    kExprCallFunction, kHook,
  ]);

  proto_config.addConfig(proto_global).addConstructor('C', ctor);
  proto_config.build();

  let constructors = {};
  let instance = builder.instantiate(
      {p: {p0: {}}, env: {hook}, c: {constructors}},
      {builtins: ['js-string', 'js-prototypes']});
  return {C: constructors.C, wasm: instance.exports};
}

function makeInstanceWithProxy(target, handlerFactory) {
  let state = {hook: () => 11};
  let {C, wasm} = makeModule(() => state.hook());
  Object.setPrototypeOf(C.prototype, new Proxy(target, handlerFactory(wasm)));
  Object.freeze(C.prototype);
  return {obj: C(), state};
}

// 1. Indexed "get" trap mutates stable-map target to non-configurable read-only
// data property with conflicting value -> must throw TypeError on lazy deopt.
(function TestGetIndexedConflicting() {
  function Target() {}
  let target = new Target();
  let {obj, state} = makeInstanceWithProxy(
      target, wasm => Object.freeze({get: wasm.geti}));
  function get(o, i) { return o[i]; }
  %PrepareFunctionForOptimization(get);
  assertEquals(11, get(obj, 0));
  assertEquals(11, get(obj, 0));
  %OptimizeFunctionOnNextCall(get);
  state.hook = () => {
    Object.defineProperty(
        target, 0, {value: 999, configurable: false, writable: false});
    return 11;
  };
  assertThrows(
      () => get(obj, 0), TypeError,
      /property '0' is a read-only and non-configurable data property/);
})();

// 2. Indexed "get" trap mutates stable-map target to non-configurable read-only
// data property with matching value -> must succeed and return that value.
(function TestGetIndexedMatching() {
  function Target() {}
  let target = new Target();
  let {obj, state} = makeInstanceWithProxy(
      target, wasm => Object.freeze({get: wasm.geti}));
  function get(o, i) { return o[i]; }
  %PrepareFunctionForOptimization(get);
  assertEquals(11, get(obj, 0));
  assertEquals(11, get(obj, 0));
  %OptimizeFunctionOnNextCall(get);
  state.hook = () => {
    Object.defineProperty(
        target, 0, {value: 999, configurable: false, writable: false});
    return 999;
  };
  assertEquals(999, get(obj, 0));
})();

// 3. String-keyed "get" trap mutates stable-map target inside try/catch ->
// TypeError from continuation is caught by the surrounding handler.
(function TestGetNamedCaughtInTryCatch() {
  function Target() {}
  let target = new Target();
  let {obj, state} = makeInstanceWithProxy(
      target, wasm => Object.freeze({get: wasm.getp}));
  function getCaught(o, k) {
    try {
      return o[k];
    } catch (e) {
      return e;
    }
  }
  %PrepareFunctionForOptimization(getCaught);
  assertEquals(11, getCaught(obj, 'prop'));
  assertEquals(11, getCaught(obj, 'prop'));
  %OptimizeFunctionOnNextCall(getCaught);
  state.hook = () => {
    Object.defineProperty(
        target, 'prop', {get: undefined, set: () => {}, configurable: false});
    return 11;
  };
  let caught = getCaught(obj, 'prop');
  assertInstanceof(caught, TypeError);
  assertMatches(/non-configurable accessor/, caught.message);
})();

// 4. Indexed "set" trap mutates stable-map target and returns truish (1) with
// conflicting value -> must throw TypeError (both sloppy and strict).
(function TestSetIndexedTruishConflicting() {
  function Target() {}
  let target = new Target();
  let {obj, state} = makeInstanceWithProxy(
      target, wasm => Object.freeze({set: wasm.seti}));
  state.hook = () => 1;
  function setSloppy(o, i, v) { return o[i] = v; }
  %PrepareFunctionForOptimization(setSloppy);
  assertEquals(42, setSloppy(obj, 0, 42));
  assertEquals(42, setSloppy(obj, 0, 42));
  %OptimizeFunctionOnNextCall(setSloppy);
  state.hook = () => {
    Object.defineProperty(
        target, 0, {value: 999, configurable: false, writable: false});
    return 1;
  };
  assertThrows(
      () => setSloppy(obj, 0, 42), TypeError,
      /'set' on proxy: trap returned truish for property '0'/);
})();

// 5. Indexed "set" trap mutates stable-map target and returns truish (1) with
// matching value -> must succeed and evaluate to the RHS value.
(function TestSetIndexedTruishMatching() {
  function Target() {}
  let target = new Target();
  let {obj, state} = makeInstanceWithProxy(
      target, wasm => Object.freeze({set: wasm.seti}));
  state.hook = () => 1;
  function setStrict(o, i, v) {
    'use strict';
    return o[i] = v;
  }
  %PrepareFunctionForOptimization(setStrict);
  assertEquals(999, setStrict(obj, 0, 999));
  assertEquals(999, setStrict(obj, 0, 999));
  %OptimizeFunctionOnNextCall(setStrict);
  state.hook = () => {
    Object.defineProperty(
        target, 0, {value: 999, configurable: false, writable: false});
    return 1;
  };
  assertEquals(999, setStrict(obj, 0, 999));
})();

// 6. "set" trap mutates stable-map target and returns falsish (0):
// - In strict mode, must throw kProxyTrapReturnedFalsishFor (not frozen-data).
// - In sloppy mode, must NOT throw even if target has a conflicting frozen
//   property (ES [[Set]] returns false before target.[[GetOwnProperty]]).
(function TestSetFalsishStrictAndSloppy() {
  // Strict mode:
  {
    function Target() {}
    let target = new Target();
    let {obj, state} = makeInstanceWithProxy(
        target, wasm => Object.freeze({set: wasm.setp}));
    state.hook = () => 1;
    function setStrict(o, k, v) {
      'use strict';
      return o[k] = v;
    }
    %PrepareFunctionForOptimization(setStrict);
    assertEquals(42, setStrict(obj, 'prop', 42));
    assertEquals(42, setStrict(obj, 'prop', 42));
    %OptimizeFunctionOnNextCall(setStrict);
    state.hook = () => {
      Object.defineProperty(
          target, 'prop', {value: 999, configurable: false, writable: false});
      return 0;
    };
    assertThrows(
        () => setStrict(obj, 'prop', 42), TypeError,
        /'set' on proxy: trap returned falsish for property 'prop'/);
  }

  // Sloppy mode:
  {
    function Target() {}
    let target = new Target();
    let {obj, state} = makeInstanceWithProxy(
        target, wasm => Object.freeze({set: wasm.seti}));
    state.hook = () => 1;
    function setSloppy(o, i, v) { return o[i] = v; }
    %PrepareFunctionForOptimization(setSloppy);
    assertEquals(42, setSloppy(obj, 0, 42));
    assertEquals(42, setSloppy(obj, 0, 42));
    %OptimizeFunctionOnNextCall(setSloppy);
    state.hook = () => {
      Object.defineProperty(
          target, 0, {value: 999, configurable: false, writable: false});
      return 0;
    };
    assertEquals(42, setSloppy(obj, 0, 42));
  }
})();

// 7. Even with an empty frozen target, a lazy deopt of a strict-mode caller
// during the "set" trap must not skip the falsish trap result check.
(function TestSetFrozenTargetLazyDeoptStrict() {
  let target = Object.freeze({});
  let {obj, state} = makeInstanceWithProxy(
      target, wasm => Object.freeze({set: wasm.seti}));
  state.hook = () => 1;
  function setStrict(o, i, v) {
    'use strict';
    return o[i] = v;
  }
  %PrepareFunctionForOptimization(setStrict);
  assertEquals(42, setStrict(obj, 0, 42));
  assertEquals(42, setStrict(obj, 0, 42));
  %OptimizeFunctionOnNextCall(setStrict);
  state.hook = () => {
    %DeoptimizeFunction(setStrict);
    return 0;
  };
  assertThrows(
      () => setStrict(obj, 0, 42), TypeError,
      /'set' on proxy: trap returned falsish for property '0'/);
})();

// 8. Special receivers like globalThis (JS_GLOBAL_PROXY_TYPE) must not be
// treated as suitable empty Proxy targets even when their own descriptor count
// is 0 and their map is stable.
(function TestGlobalProxyTarget() {
  Object.defineProperty(
      globalThis, 0, {value: 999, configurable: false, writable: false});
  let {obj} = makeInstanceWithProxy(
      globalThis, wasm => Object.freeze({get: wasm.geti}));
  function get(o, i) { return o[i]; }
  %PrepareFunctionForOptimization(get);
  assertThrows(() => get(obj, 0), TypeError);
  assertThrows(() => get(obj, 0), TypeError);
  %OptimizeFunctionOnNextCall(get);
  assertThrows(
      () => get(obj, 0), TypeError,
      /property '0' is a read-only and non-configurable data property/);
})();

// 9. String-keyed "set" trap mutating target to a non-configurable setter-less
// accessor inside try/catch -> TypeError is caught by the surrounding handler.
(function TestSetNamedAccessorCaughtInTryCatch() {
  function Target() {}
  let target = new Target();
  let {obj, state} = makeInstanceWithProxy(
      target, wasm => Object.freeze({set: wasm.setp}));
  state.hook = () => 1;
  function setCaught(o, k, v) {
    try {
      o[k] = v;
      return 'no throw';
    } catch (e) {
      return e;
    }
  }
  %PrepareFunctionForOptimization(setCaught);
  assertEquals('no throw', setCaught(obj, 'prop', 42));
  assertEquals('no throw', setCaught(obj, 'prop', 42));
  %OptimizeFunctionOnNextCall(setCaught);
  state.hook = () => {
    Object.defineProperty(
        target, 'prop', {get: () => 1, set: undefined, configurable: false});
    return 1;
  };
  let caught = setCaught(obj, 'prop', 42);
  assertInstanceof(caught, TypeError);
  assertMatches(
      /non-configurable and non-writable accessor property without a setter/,
      caught.message);
})();

// 10. Indexed trap with an object key or non-element key:
// - Object keys must be coerced via ToPropertyKey only once (not coerced a
//   second time in the lazy-deopt continuation).
// - Non-element keys (-1, undefined) must not bypass frozen named properties
//   on a target validated with KeyType::kIndex.
(function TestIndexedObjectAndNonElementKeys() {
  function Target() {}
  let target = new Target();
  let {obj, state} = makeInstanceWithProxy(
      target, wasm => Object.freeze({get: wasm.geti}));
  function get(o, i) { return o[i]; }
  %PrepareFunctionForOptimization(get);
  assertEquals(11, get(obj, 0));
  assertEquals(11, get(obj, 0));
  %OptimizeFunctionOnNextCall(get);
  let coercions = 0;
  let key = {
    [Symbol.toPrimitive]() {
      return coercions++ === 0 ? '0' : '1';
    },
  };
  state.hook = () => {
    Object.defineProperty(
        target, 0, {value: 999, configurable: false, writable: false});
    return 11;
  };
  assertThrows(
      () => get(obj, key), TypeError,
      /property '0' is a read-only and non-configurable data property/);
  assertEquals(1, coercions);

  let frozenNamed = Object.freeze({'-1': 999, undefined: 999});
  let {obj: obj2} = makeInstanceWithProxy(
      frozenNamed, wasm => Object.freeze({get: wasm.geti}));
  function get2(o, i) { return o[i]; }
  %PrepareFunctionForOptimization(get2);
  assertEquals(11, get2(obj2, 0));
  assertEquals(11, get2(obj2, 0));
  %OptimizeFunctionOnNextCall(get2);
  assertThrows(
      () => get2(obj2, -1), TypeError,
      /property '-1' is a read-only and non-configurable data property/);
  %OptimizeFunctionOnNextCall(get2);
  assertThrows(
      () => get2(obj2, undefined), TypeError,
      /property 'undefined' is a read-only and non-configurable data property/);
})();
