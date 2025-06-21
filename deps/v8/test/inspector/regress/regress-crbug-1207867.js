// Copyright 2021 the V8 project authors. All rights reserved.
// Use  of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

let {session, contextGroup, Protocol} = InspectorTest.start('Regression test for crbug/1207867');

contextGroup.addScript(`
function fun(x) { return x; }
fun.toString = fn => 'bar';

const arrow = x => x;
arrow.toString = fn => 'baz';

const bound = fun.bind(this);
bound.toString = fn => 'foo';

async function afun(x) { await x; }
afun.toString = fn => 'blah';

const native = Array.prototype.map;
native.toString = fn => 'meh';
`);

InspectorTest.runAsyncTestSuite([
  async function testFunctionDescription() {
    await Protocol.Runtime.enable();
    const {result: {result}} = await Protocol.Runtime.evaluate({expression: 'fun'});
    InspectorTest.logMessage(result);
    await Protocol.Runtime.disable();
  },

  async function testArrowFunctionDescription() {
    await Protocol.Runtime.enable();
    const {result: {result}} = await Protocol.Runtime.evaluate({expression: 'arrow'});
    InspectorTest.logMessage(result);
    await Protocol.Runtime.disable();
  },

  async function testBoundFunctionDescription() {
    await Protocol.Runtime.enable();
    const {result: {result}} = await Protocol.Runtime.evaluate({expression: 'bound'});
    InspectorTest.logMessage(result);
    await Protocol.Runtime.disable();
  },

  async function testAsyncFunctionDescription() {
    await Protocol.Runtime.enable();
    const {result: {result}} = await Protocol.Runtime.evaluate({expression: 'afun'});
    InspectorTest.logMessage(result);
    await Protocol.Runtime.disable();
  },

  async function testNativeFunctionDescription() {
    await Protocol.Runtime.enable();
    const {result: {result}} = await Protocol.Runtime.evaluate({expression: 'native'});
    InspectorTest.logMessage(result);
    await Protocol.Runtime.disable();
  }
]);
