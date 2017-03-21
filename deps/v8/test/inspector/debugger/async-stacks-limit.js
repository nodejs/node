// Copyright 2016 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

print('Checks that async stacks works good with different limits');

InspectorTest.addScript(`
var resolveTest;

function foo1() {
  debugger;
}

function foo2() {
  debugger;
  if (resolveTest) resolveTest();
}

function promise() {
  var resolve1;
  var p1 = new Promise(resolve => resolve1 = resolve);
  var p2 = p1.then(foo1);
  resolve1();
  return p2;
}

function twoPromises() {
  var resolve1;
  var resolve2;
  var p1 = new Promise(resolve => resolve1 = resolve);
  var p2 = new Promise(resolve => resolve2 = resolve);
  var p3 = p1.then(foo1);
  var p4 = p2.then(foo2);
  resolve1();
  resolve2();
  return Promise.all([p3, p4]);
}

function twoSetTimeout() {
  setTimeout(foo1, 0);
  setTimeout(foo2, 0);
  return new Promise(resolve => resolveTest = resolve);
}

function threeSetTimeout() {
  setTimeout(foo1, 0);
  setTimeout(foo2, 0);
  return new Promise(resolve => resolveTest = resolve);
}

function twentySetTimeout() {
  var resolve1;
  var p1 = new Promise(resolve => resolve1 = resolve);
  for (var i = 1; i <= 19; ++i)
    setTimeout('(function foo' + i + '(){debugger;})()',0);
  setTimeout(resolve1, 0);
  return p1;
}

//# sourceURL=test.js`, 7, 26);

InspectorTest.setupScriptMap();
Protocol.Debugger.onPaused(message => {
  InspectorTest.logCallFrames(message.params.callFrames);
  var asyncStackTrace = message.params.asyncStackTrace;
  while (asyncStackTrace) {
    InspectorTest.log(`-- ${asyncStackTrace.description} --`);
    InspectorTest.logCallFrames(asyncStackTrace.callFrames);
    asyncStackTrace = asyncStackTrace.parent;
  }
  InspectorTest.log('');
  Protocol.Debugger.resume();
});

Protocol.Debugger.enable();
Protocol.Debugger.setAsyncCallStackDepth({ maxDepth: 128 });
InspectorTest.runTestSuite([
  function testZeroLimit(next) {
    Protocol.Runtime.evaluate({
        expression: 'setMaxAsyncTaskStacks(0)//# sourceURL=expr.js'})
      .then(() => Protocol.Runtime.evaluate({
        expression: 'promise()//# sourceURL=expr.js', awaitPromise: true
      }))
      .then(() => cancelAllAsyncTasks())
      .then(next);
  },

  function testOneLimit(next) {
    Protocol.Runtime.evaluate({
        expression: 'setMaxAsyncTaskStacks(1)//# sourceURL=expr.js'})
      .then(() => Protocol.Runtime.evaluate({
        expression: 'promise()//# sourceURL=expr.js', awaitPromise: true
      }))
      .then(() => cancelAllAsyncTasks())
      .then(next);
  },

  function testOneLimitTwoPromises(next) {
    // Should be no async stacks because when first microtask is finished
    // it will resolve and schedule p3 - will remove async stack for scheduled
    // p2.
    Protocol.Runtime.evaluate({
        expression: 'setMaxAsyncTaskStacks(1)//# sourceURL=expr.js'})
      .then(() => Protocol.Runtime.evaluate({
        expression: 'twoPromises()//# sourceURL=expr.js', awaitPromise: true
      }))
      .then(() => cancelAllAsyncTasks())
      .then(next);
  },

  function testTwoLimitTwoPromises(next) {
    Protocol.Runtime.evaluate({
        expression: 'setMaxAsyncTaskStacks(2)//# sourceURL=expr.js'})
      .then(() => Protocol.Runtime.evaluate({
        expression: 'twoPromises()//# sourceURL=expr.js', awaitPromise: true
      }))
      .then(() => cancelAllAsyncTasks())
      .then(next);
  },

  function testOneLimitTwoSetTimeouts(next) {
    Protocol.Runtime.evaluate({
        expression: 'setMaxAsyncTaskStacks(1)//# sourceURL=expr.js'})
      .then(() => Protocol.Runtime.evaluate({
        expression: 'twoSetTimeout()//# sourceURL=expr.js', awaitPromise: true
      }))
      .then(() => cancelAllAsyncTasks())
      .then(next);
  },

  function testTwoLimitTwoSetTimeouts(next) {
    Protocol.Runtime.evaluate({
        expression: 'setMaxAsyncTaskStacks(2)//# sourceURL=expr.js'})
      .then(() => Protocol.Runtime.evaluate({
        expression: 'twoSetTimeout()//# sourceURL=expr.js', awaitPromise: true
      }))
      .then(() => cancelAllAsyncTasks())
      .then(next);
  },

  function testTenLimitTwentySetTimeouts(next) {
    Protocol.Runtime.evaluate({
        expression: 'setMaxAsyncTaskStacks(10)//# sourceURL=expr.js'})
      .then(() => Protocol.Runtime.evaluate({
        expression: 'twentySetTimeout()//# sourceURL=expr.js',
        awaitPromise: true
      }))
      .then(() => cancelAllAsyncTasks())
      .then(next);
  }
]);

function cancelAllAsyncTasks() {
  return Protocol.Debugger.setAsyncCallStackDepth({ maxDepth: 0 })
    .then(() => Protocol.Debugger.setAsyncCallStackDepth({ maxDepth: 128 }));
}
