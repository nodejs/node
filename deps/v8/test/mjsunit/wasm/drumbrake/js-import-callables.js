// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --harmony-shadow-realm --expose-gc

d8.file.execute("test/mjsunit/wasm/wasm-module-builder.js");

function makeImportedCall(importedFunction, callKind = "direct") {
  const builder = new WasmModuleBuilder();
  const signature = builder.addType(kSig_i_i);
  const importIndex = builder.addImport("m", "f", signature);
  const body = [kExprLocalGet, 0];
  if (callKind === "direct") {
    body.push(kExprCallFunction, importIndex);
  } else if (callKind === "indirect") {
    builder.addTable(kWasmAnyFunc, 1, 1);
    builder.addActiveElementSegment(0, wasmI32Const(0), [importIndex]);
    body.push(...wasmI32Const(0), kExprCallIndirect, signature, 0);
  } else {
    assertEquals("ref", callKind);
    builder.addDeclarativeElementSegment([importIndex]);
    body.push(kExprRefFunc, importIndex, kExprCallRef, signature);
  }
  builder.addFunction("main", signature).addBody(body).exportFunc();
  return builder.instantiate({m: {f: importedFunction}});
}

(function testProxyAndBoundFunctionImports() {
  const plainFunction = x => x + 1;
  assertEquals(42, makeImportedCall(plainFunction).exports.main(41));
  assertEquals(42, makeImportedCall(plainFunction.bind(null)).exports.main(41));
  assertEquals(42,
               makeImportedCall(new Proxy(plainFunction, {})).exports.main(41));
  assertEquals(
      42, makeImportedCall(new Proxy(plainFunction.bind(null), {}))
              .exports.main(41));
  assertEquals(
      42, makeImportedCall(new Proxy(plainFunction.bind(null).bind(null), {}))
              .exports.main(41));
  assertEquals(
      42, makeImportedCall(
              new Proxy(new Proxy(plainFunction.bind(null), {}), {}))
              .exports.main(41));

  let observedThis = "not-called";
  const sloppyFunction = function(x) { return x + 1; };
  const applyProxy = new Proxy(sloppyFunction, {
    apply(target, thisArg, args) {
      observedThis = thisArg;
      return Reflect.apply(target, thisArg, args);
    }
  });
  assertEquals(42, makeImportedCall(applyProxy).exports.main(41));
  assertEquals(undefined, observedThis);

  let observedBoundThis = "not-called";
  const applyProxyBound = new Proxy(sloppyFunction.bind(null), {
    apply(target, thisArg, args) {
      observedBoundThis = thisArg;
      return Reflect.apply(target, thisArg, args);
    }
  });
  assertEquals(42, makeImportedCall(applyProxyBound).exports.main(41));
  assertEquals(undefined, observedBoundThis);

  const shadowRealm = new ShadowRealm();
  const wrappedFunction = shadowRealm.evaluate("(x) => x + 1");
  assertEquals(42, makeImportedCall(wrappedFunction).exports.main(41));
  assertEquals(42,
               makeImportedCall(new Proxy(wrappedFunction, {}))
                   .exports.main(41));
})();

(function testImportReceiverConversion() {
  const sloppyFunction = function(x) {
    gc();
    assertSame(globalThis, this);
    return x + 1;
  };
  assertEquals(42, makeImportedCall(sloppyFunction).exports.main(41));
  assertEquals(42, makeImportedCall(new Proxy(sloppyFunction, {}))
                       .exports.main(41));

  const strictFunction = function(x) {
    "use strict";
    gc();
    assertEquals(undefined, this);
    return x + 1;
  };
  assertEquals(42, makeImportedCall(strictFunction).exports.main(41));
  assertEquals(42, makeImportedCall(new Proxy(strictFunction, {}))
                       .exports.main(41));

  const receiver = {value: 1};
  const boundFunction = function(x) {
    gc();
    assertSame(receiver, this);
    return x + this.value;
  }.bind(receiver);
  assertEquals(42, makeImportedCall(boundFunction).exports.main(41));
  assertEquals(42, makeImportedCall(new Proxy(boundFunction, {}))
                       .exports.main(41));
})();

(function testImportConversionRealm() {
  const realm = Realm.create();
  const foreignFunction = Realm.eval(realm, "() => Symbol()");
  for (const callKind of ["direct", "indirect", "ref"]) {
    for (const callable
             of [foreignFunction, foreignFunction.bind(null),
                 new Proxy(foreignFunction, {})]) {
      const instance = makeImportedCall(callable, callKind);
      assertThrows(() => instance.exports.main(0), TypeError);
    }
  }
})();

(function testRevokedProxyImports() {
  for (const callKind of ["direct", "indirect", "ref"]) {
    const {proxy, revoke} = Proxy.revocable(x => x + 1, {});
    const instance = makeImportedCall(proxy, callKind);
    assertEquals(42, instance.exports.main(41));
    revoke();
    assertThrows(() => instance.exports.main(41), TypeError, /revoked/);
  }
})();
