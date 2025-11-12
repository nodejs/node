// Copyright 2021 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

let {session, contextGroup, Protocol} =
  InspectorTest.start('Tests side-effect-free Runtime.callFunctionOn()');

async function check(callable, object, ...args) {
  const functionDeclaration = callable.toString();
  const {result:{exceptionDetails}} = await Protocol.Runtime.callFunctionOn({
    objectId: object.objectId,
    functionDeclaration,
    arguments: args.map(arg => (arg && arg.objectId) ? {objectId: arg.objectId} : {value: arg}),
    throwOnSideEffect: true,
  });
  InspectorTest.log(`${functionDeclaration}: ${exceptionDetails ? 'throws' : 'ok'}`);
};

InspectorTest.runAsyncTestSuite([
  async function testCallFunctionOnSideEffectFree() {
    await Protocol.Runtime.enable();
    const {result: {result: object}} = await Protocol.Runtime.evaluate({ expression: '({a: 1, b: ""})'});
    await check(function getA() { return this.a; }, object);
    await check(function setA(a) { this.a = a; }, object, 1);
    await check(function getB() { return this.b; }, object);
    await check(function setB(b) { this.b = b; }, object, "lala");
    await check(function setSomeGlobal() { globalThis.someGlobal = this; }, object);
    await check(function localSideEffect() { const date = new Date(); date.setDate(this.a); return date; }, object);
    await Protocol.Runtime.disable();
  }
]);
