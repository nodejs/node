// Copyright 2018 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

let {session, contextGroup, Protocol} = InspectorTest.start(
    'Tests Command Line API evaluations with throwOnSideEffect.');

session.setupScriptMap();
contextGroup.addScript(`
    function f() {
    }
    var obj = {x: 1, y: 2};
    //# sourceURL=test.js`);
Protocol.Runtime.enable();
Protocol.Debugger.enable();

(async function() {
  utils.setMemoryInfoForTest(42);
  await testExpression('console.memory');
  await Protocol.Runtime.evaluate(
      {expression: '43', objectGroup: 'console', includeCommandLineAPI: true});
  await testExpression('$_');
  await testExpression('$0');
  await testExpression('$1');
  await testExpression('$2');
  await testExpression('$3');
  await testExpression('$4');
  await testMethod('console.debug');
  await testMethod('console.error');
  await testMethod('console.info');
  await testMethod('console.log');
  await testMethod('console.warn');
  await testMethod('console.dir');
  await testMethod('console.dirxml');
  await testMethod('console.table');
  await testMethod('console.trace');
  await testMethod('console.group');
  await testMethod('console.groupEnd');
  await testMethod('console.groupCollapsed');
  await testMethod('console.clear');
  await testMethod('console.count');
  await testMethod('console.assert');
  await testMethod('console.profile');
  await testMethod('console.profileEnd');
  await testMethod('console.time');
  await testMethod('console.timeEnd');
  await testMethod('debug', ['f']);
  await testMethod('undebug', ['f']);
  await testMethod('monitor');
  await testMethod('unmonitor');
  await testMethod('keys', ['obj']);
  await testMethod('values', ['obj']);
  await testMethod('inspect');
  await testMethod('copy', ['1']);
  await testMethod('queryObjects', ['Array']);

  InspectorTest.completeTest();

  async function testExpression(expression) {
    InspectorTest.log(`\nExpression: ${expression}`);
    await evaluateAndPrint(expression);
  }

  async function testMethod(method, args = []) {
    InspectorTest.log(`\nMethod: ${method}`);
    await evaluateAndPrint(`${method}(${args.join(', ')})`);
    await evaluateAndPrint(`${method}.toString()`);
  }

  async function evaluateAndPrint(expression) {
    const result = (await Protocol.Runtime.evaluate({
                     expression,
                     throwOnSideEffect: true,
                     includeCommandLineAPI: true
                   })).result;
    if (result.exceptionDetails)
      InspectorTest.logMessage(result.exceptionDetails.exception.description);
    else if (result.result)
      InspectorTest.logMessage(
          result.result.description || result.result.value ||
          result.result.type);
  }
})();
