// Copyright 2017 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

let {session, contextGroup, Protocol} = InspectorTest.start('Tests side-effect-free evaluation');

contextGroup.addScript(`
var someGlobalDate = new Date();
function testFunction()
{
  var o = 0;
  function f() { return 1; }
  function g() { o = 2; return o; }
  f,g;
  debugger;
}
//# sourceURL=foo.js`);

InspectorTest.runAsyncTestSuite([
  async function basicTest() {
    Protocol.Debugger.enable();
    Protocol.Runtime.evaluate({ 'expression': 'setTimeout(testFunction, 0)' });
    const {params:{callFrames:[{callFrameId: topFrameId}]}} = await Protocol.Debugger.oncePaused();
    InspectorTest.log('Paused on "debugger;"');
    const {result:{result:{value: fResult}}} = await Protocol.Debugger.evaluateOnCallFrame({ callFrameId: topFrameId, expression: 'f()' });
    InspectorTest.log('f() returns ' + fResult);
    const {result:{result:{value: gResult}}} = await Protocol.Debugger.evaluateOnCallFrame({ callFrameId: topFrameId, expression: 'g()' });
    InspectorTest.log('g() returns ' + gResult);
    const {result:{result:{value: fResultSideEffect}}} = await Protocol.Debugger.evaluateOnCallFrame({ callFrameId: topFrameId, expression: 'f()', throwOnSideEffect: true});
    InspectorTest.log('f() returns ' + fResultSideEffect);
    const {result:{result:{className}}} = await Protocol.Debugger.evaluateOnCallFrame({ callFrameId: topFrameId, expression: 'g()', throwOnSideEffect: true});
    InspectorTest.log('g() throws ' + className);
  },

  async function testDate() {
    const check = async (expression) => {
      const {result:{exceptionDetails}} = await Protocol.Runtime.evaluate({expression, throwOnSideEffect: true});
      InspectorTest.log(expression + ' : ' + (exceptionDetails ? 'throws' : 'ok'));
    };
    // setters are only ok on temporary objects
    await check('someGlobalDate.setDate(10)');
    await check('new Date().setDate(10)');
    await check('someGlobalDate.setFullYear(1991)');
    await check('new Date().setFullYear(1991)');
    await check('someGlobalDate.setHours(0)');
    await check('new Date().setHours(0)');
    // getters are ok on any Date
    await check('someGlobalDate.getDate()');
    await check('new Date().getDate()');
    await check('someGlobalDate.getFullYear()');
    await check('new Date().getFullYear()');
    await check('someGlobalDate.getHours()');
    await check('new Date().getHours()');
  }
]);
