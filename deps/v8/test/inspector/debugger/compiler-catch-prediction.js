// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax

// Test debug event catch prediction for thrown exceptions. Exceptions are
// classified as "caught" or "uncaught" based on the following assumptions:
//  1) try-catch   : Will always catch the exception.
//  2) try-finally : Will always re-throw the exception.

const {session, contextGroup, Protocol} = InspectorTest.start(
    'Test debug event catch prediction for thrown exceptions.');

contextGroup.addScript(`
function TryCatch() {
  function f(a) {
    try {
      throw "boom" + a;
    } catch(e) {
      return e;
    }
  }
  %PrepareFunctionForOptimization(f);
  console.log(f(1));
  console.log(f(2));
  %OptimizeFunctionOnNextCall(f);
  console.log(f(3));
}

function TryFinally() {
  function f(a) {
    try {
      throw "baem" + a;
    } finally {
      return a + 10;
    }
  }
  %PrepareFunctionForOptimization(f);
  console.log(f(1));
  console.log(f(2));
  %OptimizeFunctionOnNextCall(f);
  console.log(f(3));
}

function TryCatchFinally() {
  function f(a) {
    try {
      throw "wosh" + a;
    } catch(e) {
      return e + a;
    } finally {
      // Nothing.
    }
  }
  %PrepareFunctionForOptimization(f);
  console.log(f(1));
  console.log(f(2));
  %OptimizeFunctionOnNextCall(f);
  console.log(f(3));
}

function TryCatchNestedFinally() {
  function f(a) {
    try {
      try {
        throw "bang" + a;
      } finally {
        // Nothing.
      }
    } catch(e) {
      return e + a;
    }
  }
  %PrepareFunctionForOptimization(f);
  console.log(f(1));
  console.log(f(2));
  %OptimizeFunctionOnNextCall(f);
  console.log(f(3));
}

function TryFinallyNestedCatch() {
  function f(a) {
    try {
      try {
        throw "peng" + a;
      } catch(e) {
        return e
      }
    } finally {
      return a + 10;
    }
  }
  %PrepareFunctionForOptimization(f);
  console.log(f(1));
  console.log(f(2));
  %OptimizeFunctionOnNextCall(f);
  console.log(f(3));
}

function TryFinallyNestedFinally() {
  function f(a) {
    try {
      try {
        throw "oops" + a;
      } finally {
        // Nothing.
      }
    } finally {
      return a + 10;
    }
  }
  %PrepareFunctionForOptimization(f);
  console.log(f(1));
  console.log(f(2));
  %OptimizeFunctionOnNextCall(f);
  console.log(f(3));
}
`);

(async function test() {
  await Protocol.Runtime.enable();
  await Protocol.Debugger.enable();
  await Protocol.Debugger.setPauseOnExceptions({state: 'all'});

  let log = [];
  Protocol.Runtime.onConsoleAPICalled(({params}) => {
    InspectorTest.log(`result: ${params.args[0].value}`);
  });
  Protocol.Debugger.onPaused(({params}) => {
    if (params.reason === 'exception') {
      log.push([params.data.value, params.data.uncaught]);
    }
    Protocol.Debugger.resume();
  });

  for (const name
           of ['TryCatch',
               'TryFinally',
               'TryCatchFinally',
               'TryCatchNestedFinally',
               'TryFinallyNestedCatch',
               'TryFinallyNestedFinally',
  ]) {
    InspectorTest.log(`Running ${name}:`);
    log = [];
    await Protocol.Runtime.evaluate({expression: `setTimeout(${name}, 0)`});
    await InspectorTest.waitForPendingTasks();
    InspectorTest.log(`Collect log: ${JSON.stringify(log)}`);
  }

  InspectorTest.completeTest();
})();
