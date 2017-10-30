// Copyright 2017 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// TODO(kozyatinskiy): fix this test.
let {session, contextGroup, Protocol} = InspectorTest.start('Checks created frame for async call chain');

contextGroup.addScript(
    `
function foo1() {
  debugger;
}

function foo2() {
  debugger;
}

function promise() {
  var resolve;
  var p1 = new Promise(r => resolve = r);
  var p2 = p1.then(foo1);
  resolve();
  return p2;
}

function promiseThen() {
  var resolve;
  var p1 = new Promise(r => resolve = r);
  var p2 = p1.then(foo1);
  var p3 = p2.then(foo2);
  resolve();
  return p3;
}

function promiseThenThen() {
  var resolve;
  var p1 = new Promise(r => resolve = r);
  var p2 = p1.then(foo1).then(foo2);
  var p3 = p1.then(foo1);
  resolve();
  return p2;
}

function promiseResolve() {
  return Promise.resolve().then(foo1);
}

function promiseReject() {
  return Promise.reject().catch(foo1);
}

function promiseAll() {
  return Promise.all([ Promise.resolve() ]).then(foo1);
}

function promiseRace() {
  return Promise.race([ Promise.resolve() ]).then(foo1);
}

function thenableJob1() {
  return Promise.resolve().then(() => Promise.resolve().then(() => 42)).then(foo1);
}

function thenableJob2() {
  return Promise.resolve().then(() => Promise.resolve()).then(foo1);
}

function setTimeouts() {
  var resolve;
  var p = new Promise(r => resolve = r);
  setTimeout(() =>
    setTimeout(() =>
      setTimeout(() => { foo1(); resolve(); }, 0), 0), 0);
  return p;
}

//# sourceURL=test.js`,
    8, 4);

session.setupScriptMap();
Protocol.Debugger.onPaused(message => {
  session.logCallFrames(message.params.callFrames);
  session.logAsyncStackTrace(message.params.asyncStackTrace);
  InspectorTest.log('');
  Protocol.Debugger.resume();
});

Protocol.Debugger.enable();
Protocol.Debugger.setAsyncCallStackDepth({maxDepth: 128});

InspectorTest.runTestSuite([
  function testPromise(next) {
    Protocol.Runtime
        .evaluate(
            {expression: 'promise()//# sourceURL=expr.js', awaitPromise: true})
        .then(next);
  },

  function testPromiseThen(next) {
    Protocol.Runtime
        .evaluate({
          expression: 'promiseThen()//# sourceURL=expr.js',
          awaitPromise: true
        })
        .then(next);
  },

  function testPromiseThenThen(next) {
    Protocol.Runtime
        .evaluate({
          expression: 'promiseThenThen()//# sourceURL=expr.js',
          awaitPromise: true
        })
        .then(next);
  },

  function testPromiseResolve(next) {
    Protocol.Runtime
        .evaluate({
          expression: 'promiseResolve()//# sourceURL=expr.js',
          awaitPromise: true
        })
        .then(next);
  },

  function testPromiseReject(next) {
    Protocol.Runtime
        .evaluate({
          expression: 'promiseReject()//# sourceURL=expr.js',
          awaitPromise: true
        })
        .then(next);
  },

  function testPromiseAll(next) {
    Protocol.Runtime
        .evaluate({
          expression: 'promiseAll()//# sourceURL=expr.js',
          awaitPromise: true
        })
        .then(next);
  },

  function testPromiseRace(next) {
    Protocol.Runtime
        .evaluate({
          expression: 'promiseRace()//# sourceURL=expr.js',
          awaitPromise: true
        })
        .then(next);
  },

  function testThenableJob1(next) {
    Protocol.Runtime
        .evaluate({
          expression: 'thenableJob1()//# sourceURL=expr.js',
          awaitPromise: true
        })
        .then(next);
  },

  function testThenableJob2(next) {
    Protocol.Runtime
        .evaluate({
          expression: 'thenableJob2()//# sourceURL=expr.js',
          awaitPromise: true
        })
        .then(next);
  },

  function testSetTimeouts(next) {
    Protocol.Runtime
        .evaluate({
          expression: 'setTimeouts()//# sourceURL=expr.js',
          awaitPromise: true
        })
        .then(next);
  }
]);
