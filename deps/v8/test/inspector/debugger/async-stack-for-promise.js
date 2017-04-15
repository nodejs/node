// Copyright 2016 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

print('Checks that async chains for promises are correct.');

InspectorTest.addScript(`
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

function promiseResolvedBySetTimeout() {
  var resolve;
  var p1 = new Promise(r => resolve = r);
  var p2 = p1.then(foo1);
  setTimeout(resolve, 0);
  return p2;
}

function promiseAll() {
  var resolve1;
  var resolve2;
  var p1 = new Promise(resolve => resolve1 = resolve);
  var p2 = new Promise(resolve => resolve2 = resolve);
  var p3 = Promise.all([ p1, p2 ]).then(foo1);
  resolve1();
  resolve2();
  return p3;
}

function promiseAllReverseOrder() {
  var resolve1;
  var resolve2;
  var p1 = new Promise(resolve => resolve1 = resolve);
  var p2 = new Promise(resolve => resolve2 = resolve);
  var p3 = Promise.all([ p1, p2 ]).then(foo1);
  resolve2();
  resolve1();
  return p3;
}

function promiseRace() {
  var resolve1;
  var resolve2;
  var p1 = new Promise(resolve => resolve1 = resolve);
  var p2 = new Promise(resolve => resolve2 = resolve);
  var p3 = Promise.race([ p1, p2 ]).then(foo1);
  resolve1();
  resolve2();
  return p3;
}

function twoChainedCallbacks() {
  var resolve1;
  var p1 = new Promise(resolve => resolve1 = resolve);
  var p2 = p1.then(foo1).then(foo2);
  resolve1();
  return p2;
}

function promiseResolve() {
  return Promise.resolve().then(foo1).then(foo2);
}

function thenableJobResolvedInSetTimeout() {
  function thenableJob() {
    var resolve2;
    var p2 = new Promise(resolve => resolve2 = resolve);
    setTimeout(resolve2, 0);
    return p2;
  }
  var resolve1;
  var p1 = new Promise(resolve => resolve1 = resolve);
  var p3 = p1.then(() => thenableJob()).then(foo1);
  resolve1();
  return p3;
}

function thenableJobResolvedInSetTimeoutWithStack() {
  function thenableJob() {
    function inner() {
      resolve2();
    }

    var resolve2;
    var p2 = new Promise(resolve => resolve2 = resolve);
    setTimeout(inner, 0);
    return p2;
  }
  var resolve1;
  var p1 = new Promise(resolve => resolve1 = resolve);
  var p3 = p1.then(() => thenableJob()).then(foo1);
  resolve1();
  return p3;
}

function thenableJobResolvedByPromise() {
  function thenableJob() {
    var resolve2;
    var p2 = new Promise(resolve => resolve2 = resolve);
    Promise.resolve().then(resolve2);
    return p2;
  }
  var resolve1;
  var p1 = new Promise(resolve => resolve1 = resolve);
  var p3 = p1.then(() => thenableJob()).then(foo1);
  resolve1();
  return p3;
}

function thenableJobResolvedByPromiseWithStack() {
  function thenableJob() {
    function inner() {
      resolve2();
    }

    var resolve2;
    var p2 = new Promise(resolve => resolve2 = resolve);
    Promise.resolve().then(inner);
    return p2;
  }
  var resolve1;
  var p1 = new Promise(resolve => resolve1 = resolve);
  var p3 = p1.then(() => thenableJob()).then(foo1);
  resolve1();
  return p3;
}

function lateThenCallback() {
  var resolve1;
  var p1 = new Promise(resolve => resolve1 = resolve);
  resolve1();
  return p1.then(foo1);
}

function complex() {
  var testResolve;
  var testPromise = new Promise(resolve => testResolve = resolve);

  function foo1() {
    function inner1() {
      debugger;
    }
    inner1();
  }

  function foo2() {
    var resolve20;
    function inner2() {
      resolve20();
    }
    var p20 = new Promise(resolve => resolve20 = resolve);
    Promise.resolve().then(inner2);
    return p20;
  }

  function foo3() {
    var resolve17;
    function inner3() {
      resolve17();
    }
    var p17 = new Promise(resolve => resolve17 = resolve);
    setTimeout(inner3, 0);
    return p17;
  }

  function foo4() {
    function inner4() {
      return;
    }
    return inner4();
  }

  function foo5() {
    return Promise.all([ Promise.resolve(), Promise.resolve() ])
      .then(() => 42);
  }

  function foo6() {
    return Promise.race([ Promise.resolve(), Promise.resolve()])
      .then(() => 42);
  }

  var p = Promise.resolve()
    .then(foo6)
    .then(foo5)
    .then(foo4)
    .then(foo3)
    .then(foo2)
    .then(foo1);

  setTimeout(() => {
    p.then(() => {
      p.then(() => {
        debugger;
        testResolve();
      })
    })
  }, 0)

  return testPromise;
}

function reject() {
  return Promise.reject().catch(foo1);
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
var testList = [
  'promise',
  'promiseResolvedBySetTimeout',
  'promiseAll',
  'promiseAllReverseOrder',
  'promiseRace',
  'twoChainedCallbacks',
  'promiseResolve',
  'thenableJobResolvedInSetTimeout',
  'thenableJobResolvedInSetTimeoutWithStack',
  'thenableJobResolvedByPromise',
  'thenableJobResolvedByPromiseWithStack',
  'lateThenCallback',
  'complex',
  'reject',
]
InspectorTest.runTestSuite(testList.map(name => {
  return eval(`
  (function test${capitalize(name)}(next) {
    Protocol.Runtime.evaluate({ expression: \`${name}()
//# sourceURL=test${capitalize(name)}.js\`, awaitPromise: true})
      .then(next);
  })
  `);
}));

function capitalize(string) {
  return string.charAt(0).toUpperCase() + string.slice(1);
}
