// Copyright 2016 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

let {session, contextGroup, Protocol} = InspectorTest.start('Checks that async chains for for-await-of are correct.');

contextGroup.addInlineScript(`

function Debugger(value) {
  debugger;
}

function Reject(reason) {
  var reject;
  var promise = new Promise(function(resolvefn, rejectfn) {
    reject = rejectfn;
  });
  setTimeout(reject.bind(undefined, reason), 0);
  return promise;
}

function Throw(reason) {
  return {
    get then() { throw reason; }
  };
}

function ThrowOnReturn(items) {
  var it = items[Symbol.iterator]();
  return {
    [Symbol.iterator]() { return this; },
    next(v) { return it.next(v); },
    return(v) { throw new Error("boop"); }
  };
}

function RejectOnReturn(items) {
  var it = items[Symbol.iterator]();
  return {
    [Symbol.iterator]() { return this; },
    next(v) { return it.next(v); },
    return(v) { return Reject(new Error("boop")); }
  };
}

async function Basic() {
  for await (let x of ["a"]) {
    Debugger();
  }
}

async function UncaughtReject() {
  async function loop() {
    for await (let x of [Reject(new Error("boop"))]) {
      Debugger();
    }
  }
  return loop().catch(Debugger);
}

async function UncaughtThrow() {
  async function loop() {
    for await (let x of [Throw(new Error("boop"))]) {
      Debugger();
    }
  }
  return loop().catch(Debugger);
}

async function CaughtReject() {
  try {
    for await (let x of [Reject(new Error("boop"))]) {
      Debugger(x);
    }
  } catch (e) {
    Debugger(e);
  }
}

async function CaughtThrow() {
  try {
    for await (let x of [Throw(new Error("boop"))]) {
      Debugger(x);
    }
  } catch (e) {
    Debugger(e);
  }
}

async function UncaughtRejectOnBreak() {
  async function loop() {
    for await (let x of RejectOnReturn(["0", "1"])) {
      break;
    }
  }
  return loop().catch(Debugger);
}

async function UncaughtThrowOnBreak() {
  async function loop() {
    for await (let x of ThrowOnReturn(["0", "1"])) {
      break;
    }
  }
  return loop().catch(Debugger);
}

async function CaughtRejectOnBreak() {
  try {
    for await (let x of RejectOnReturn(["0", "1"])) {
      break;
    }
  } catch (e) {
    Debugger(e);
  }
}

async function CaughtThrowOnBreak() {
  try {
    for await (let x of ThrowOnReturn(["0", "1"])) {
      break;
    }
  } catch (e) {
    Debugger(e);
  }
}`, 'test.js');

session.setupScriptMap();
Protocol.Debugger.onPaused(message => {
  session.logCallFrames(message.params.callFrames);
  session.logAsyncStackTrace(message.params.asyncStackTrace);
  InspectorTest.log('');
  Protocol.Debugger.resume();
});

Protocol.Debugger.enable();
Protocol.Debugger.setAsyncCallStackDepth({ maxDepth: 128 });
var testList = [
  'Basic',
  'UncaughtReject',
  'UncaughtThrow',
  'CaughtReject',
  'CaughtThrow',
  'UncaughtRejectOnBreak',
  'UncaughtThrowOnBreak',
  'CaughtRejectOnBreak',
  'CaughtThrowOnBreak',
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
