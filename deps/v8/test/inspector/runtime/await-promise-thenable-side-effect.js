// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

const {session, contextGroup, Protocol} = InspectorTest.start(
    'Tests that awaitPromise enforces throwOnSideEffect without microtasks');

const testSetupScript = `
  globalThis.sideEffects = 0;
  globalThis.microtaskRun = 0;
  function reset() {
    globalThis.sideEffects = 0;
    globalThis.microtaskRun = 0;
  }
  const thenableGetter = {
    get then() {
      globalThis.sideEffects++;
      return undefined;
    }
  };
  const thenableMethod = {
    then(resolve) {
      globalThis.sideEffects++;
      resolve(42);
    }
  };
  const proxyThenable = new Proxy({}, {
    get(target, prop) {
      if (prop === 'then') {
        globalThis.sideEffects++;
      }
      return undefined;
    }
  });
  const nestedThenable = {
    then(resolve) {
      resolve({
        get then() {
          globalThis.sideEffects++;
          return undefined;
        }
      });
    }
  };
  const nestedMethodThenable = {
    then(resolve) {
      resolve({
        then(r) {
          globalThis.sideEffects++;
          r(42);
        }
      });
    }
  };
  const pureThenable = {
    then(resolve) {
      resolve(42);
    }
  };
  const resolvedPromise = Promise.resolve(42);
`;

contextGroup.addScript(testSetupScript);

async function logResult(label, response) {
  const {result} = response;
  if (result.exceptionDetails) {
    InspectorTest.log(
        `${label}: exception (${result.result.description.split('\n')[0]})`);
  } else {
    InspectorTest.log(`${label}: ok (${
        result.result.value !== undefined ?
            JSON.stringify(result.result.value) :
            result.result.type})`);
  }
}

async function checkSideEffects(label) {
  const {result} = await Protocol.Runtime.evaluate({
    expression: 'globalThis.sideEffects',
    returnByValue: true,
  });
  InspectorTest.log(`${label} sideEffects: ${result.result.value}`);
  await Protocol.Runtime.evaluate({expression: 'reset()'});
}

InspectorTest.runAsyncTestSuite([
  async function testRuntimeEvaluateFulfilledPromise() {
    await Protocol.Runtime.evaluate({expression: 'reset()'});

    await logResult('resolvedPromise', await Protocol.Runtime.evaluate({
      expression: 'resolvedPromise',
      awaitPromise: true,
      throwOnSideEffect: true,
    }));

    await logResult('(async () => 42)()', await Protocol.Runtime.evaluate({
      expression: '(async () => 42)()',
      awaitPromise: true,
      throwOnSideEffect: true,
    }));
  },

  async function testRuntimeEvaluateNonPromiseOrUnfulfilled() {
    await Protocol.Runtime.evaluate({expression: 'reset()'});

    await logResult('primitive 42', await Protocol.Runtime.evaluate({
      expression: '42',
      awaitPromise: true,
      throwOnSideEffect: true,
    }));

    await logResult('plain object', await Protocol.Runtime.evaluate({
      expression: '({ a: 1 })',
      awaitPromise: true,
      throwOnSideEffect: true,
    }));

    await logResult('thenableGetter', await Protocol.Runtime.evaluate({
      expression: 'thenableGetter',
      awaitPromise: true,
      throwOnSideEffect: true,
    }));
    await checkSideEffects('thenableGetter');

    await logResult('thenableMethod', await Protocol.Runtime.evaluate({
      expression: 'thenableMethod',
      awaitPromise: true,
      throwOnSideEffect: true,
    }));
    await checkSideEffects('thenableMethod');

    await logResult('proxyThenable', await Protocol.Runtime.evaluate({
      expression: 'proxyThenable',
      awaitPromise: true,
      throwOnSideEffect: true,
    }));
    await checkSideEffects('proxyThenable');

    await logResult('pureThenable', await Protocol.Runtime.evaluate({
      expression: 'pureThenable',
      awaitPromise: true,
      throwOnSideEffect: true,
    }));
    await checkSideEffects('pureThenable');

    await logResult('nestedThenable', await Protocol.Runtime.evaluate({
      expression: 'nestedThenable',
      awaitPromise: true,
      throwOnSideEffect: true,
    }));
    await checkSideEffects('nestedThenable');

    await logResult('nestedMethodThenable', await Protocol.Runtime.evaluate({
      expression: 'nestedMethodThenable',
      awaitPromise: true,
      throwOnSideEffect: true,
    }));
    await checkSideEffects('nestedMethodThenable');

    await logResult('pending promise', await Protocol.Runtime.evaluate({
      expression: 'new Promise(() => {})',
      awaitPromise: true,
      throwOnSideEffect: true,
    }));

    await logResult('rejected promise', await Protocol.Runtime.evaluate({
      expression: 'Promise.reject(\'err\')',
      awaitPromise: true,
      throwOnSideEffect: true,
    }));
  },

  async function testReplMode() {
    await Protocol.Runtime.evaluate({expression: 'reset()'});

    await logResult('repl 1 + 2', await Protocol.Runtime.evaluate({
      expression: '1 + 2',
      replMode: true,
      awaitPromise: true,
      throwOnSideEffect: true,
    }));

    await logResult('repl \'hello\'', await Protocol.Runtime.evaluate({
      expression: '\'hello\'',
      replMode: true,
      awaitPromise: true,
      throwOnSideEffect: true,
    }));

    await logResult('repl let x = 1', await Protocol.Runtime.evaluate({
      expression: 'let x = 1',
      replMode: true,
      awaitPromise: true,
      throwOnSideEffect: true,
    }));

    const {error} = await Protocol.Runtime.evaluate({
      expression: 'while (true);',
      replMode: true,
      throwOnSideEffect: true,
      timeout: 100,
    });
    InspectorTest.log(`repl timeout: error (${error.message})`);
  },

  async function testAsyncFunctionReturningThenableOrPromise() {
    await Protocol.Runtime.evaluate({expression: 'reset()'});

    for (const awaitPromise of [true, false]) {
      const prefix = `awaitPromise=${awaitPromise}`;

      await logResult(
          `${prefix}: async returning thenableMethod`,
          await Protocol.Runtime.evaluate({
            expression: '(async () => thenableMethod)()',
            awaitPromise,
            throwOnSideEffect: true,
          }));
      await checkSideEffects(`${prefix}: async returning thenableMethod`);

      await logResult(
          `${prefix}: async returning thenableGetter`,
          await Protocol.Runtime.evaluate({
            expression: '(async () => thenableGetter)()',
            awaitPromise,
            throwOnSideEffect: true,
          }));
      await checkSideEffects(`${prefix}: async returning thenableGetter`);

      await logResult(
          `${prefix}: async returning resolvedPromise`,
          await Protocol.Runtime.evaluate({
            expression: '(async () => resolvedPromise)()',
            awaitPromise,
            throwOnSideEffect: true,
          }));
      await checkSideEffects(`${prefix}: async returning resolvedPromise`);
    }
  },

  async function testMicrotasksNotDrainedDuringThrowOnSideEffect() {
    await Protocol.Runtime.evaluate({expression: 'reset()'});
    await Protocol.Debugger.enable();

    Protocol.Runtime.evaluate({
      expression:
          'Promise.resolve().then(() => { globalThis.microtaskRun++; }); debugger;',
    });

    await Protocol.Debugger.oncePaused();

    // Evaluate with throwOnSideEffect: true and awaitPromise: true.
    await logResult(
        'side-effect-free evaluate while paused',
        await Protocol.Runtime.evaluate({
          expression: 'resolvedPromise',
          awaitPromise: true,
          throwOnSideEffect: true,
        }));

    // Verify microtask was NOT run during the side-effect evaluation.
    const {result: check1} = await Protocol.Runtime.evaluate({
      expression: 'globalThis.microtaskRun',
      returnByValue: true,
      throwOnSideEffect: true,
    });
    InspectorTest.log(`microtasks run while paused: ${check1.result.value}`);

    await Protocol.Debugger.resume();

    // Draining microtasks after resume runs the queued microtask.
    const {result: check2} = await Protocol.Runtime.evaluate({
      expression: 'globalThis.microtaskRun',
      returnByValue: true,
    });
    InspectorTest.log(`microtasks run after resume: ${check2.result.value}`);

    await Protocol.Debugger.disable();
  },

  async function testRuntimeCallFunctionOn() {
    await Protocol.Runtime.evaluate({expression: 'reset()'});
    const {result: {result: {objectId}}} = await Protocol.Runtime.evaluate({
      expression: '({})',
    });

    await logResult(
        'callFunctionOn resolvedPromise',
        await Protocol.Runtime.callFunctionOn({
          objectId,
          functionDeclaration: 'function() { return resolvedPromise; }',
          awaitPromise: true,
          throwOnSideEffect: true,
        }));

    await logResult(
        'callFunctionOn async 42', await Protocol.Runtime.callFunctionOn({
          objectId,
          functionDeclaration: 'async function() { return 42; }',
          awaitPromise: true,
          throwOnSideEffect: true,
        }));

    await logResult(
        'callFunctionOn primitive', await Protocol.Runtime.callFunctionOn({
          objectId,
          functionDeclaration: 'function() { return 42; }',
          awaitPromise: true,
          throwOnSideEffect: true,
        }));

    await logResult(
        'callFunctionOn plainObject', await Protocol.Runtime.callFunctionOn({
          objectId,
          functionDeclaration: 'function() { return { a: 1 }; }',
          awaitPromise: true,
          throwOnSideEffect: true,
        }));

    await logResult(
        'callFunctionOn thenableGetter', await Protocol.Runtime.callFunctionOn({
          objectId,
          functionDeclaration: 'function() { return thenableGetter; }',
          awaitPromise: true,
          throwOnSideEffect: true,
        }));
    await checkSideEffects('callFunctionOn thenableGetter');

    await logResult(
        'callFunctionOn thenableMethod', await Protocol.Runtime.callFunctionOn({
          objectId,
          functionDeclaration: 'function() { return thenableMethod; }',
          awaitPromise: true,
          throwOnSideEffect: true,
        }));
    await checkSideEffects('callFunctionOn thenableMethod');

    await logResult(
        'callFunctionOn proxyThenable', await Protocol.Runtime.callFunctionOn({
          objectId,
          functionDeclaration: 'function() { return proxyThenable; }',
          awaitPromise: true,
          throwOnSideEffect: true,
        }));
    await checkSideEffects('callFunctionOn proxyThenable');

    await logResult(
        'callFunctionOn pureThenable', await Protocol.Runtime.callFunctionOn({
          objectId,
          functionDeclaration: 'function() { return pureThenable; }',
          awaitPromise: true,
          throwOnSideEffect: true,
        }));
    await checkSideEffects('callFunctionOn pureThenable');

    await logResult(
        'callFunctionOn nestedThenable', await Protocol.Runtime.callFunctionOn({
          objectId,
          functionDeclaration: 'function() { return nestedThenable; }',
          awaitPromise: true,
          throwOnSideEffect: true,
        }));
    await checkSideEffects('callFunctionOn nestedThenable');

    await logResult(
        'callFunctionOn nestedMethodThenable',
        await Protocol.Runtime.callFunctionOn({
          objectId,
          functionDeclaration: 'function() { return nestedMethodThenable; }',
          awaitPromise: true,
          throwOnSideEffect: true,
        }));
    await checkSideEffects('callFunctionOn nestedMethodThenable');

    await logResult(
        'callFunctionOn pending promise',
        await Protocol.Runtime.callFunctionOn({
          objectId,
          functionDeclaration: 'function() { return new Promise(() => {}); }',
          awaitPromise: true,
          throwOnSideEffect: true,
        }));

    await logResult(
        'callFunctionOn rejected promise',
        await Protocol.Runtime.callFunctionOn({
          objectId,
          functionDeclaration: 'function() { return Promise.reject(\'err\'); }',
          awaitPromise: true,
          throwOnSideEffect: true,
        }));

    await logResult(
        'callFunctionOn async returning thenableMethod',
        await Protocol.Runtime.callFunctionOn({
          objectId,
          functionDeclaration: 'async function() { return thenableMethod; }',
          awaitPromise: true,
          throwOnSideEffect: true,
        }));
    await checkSideEffects('callFunctionOn async returning thenableMethod');

    await logResult(
        'callFunctionOn async returning resolvedPromise',
        await Protocol.Runtime.callFunctionOn({
          objectId,
          functionDeclaration: 'async function() { return resolvedPromise; }',
          awaitPromise: true,
          throwOnSideEffect: true,
        }));
    await checkSideEffects('callFunctionOn async returning resolvedPromise');
  },

  async function testPauseOnExceptionsSuppressed() {
    await Protocol.Runtime.evaluate({expression: 'reset()'});
    await Protocol.Debugger.enable();
    await Protocol.Debugger.setPauseOnExceptions({state: 'all'});

    let pausedCount = 0;
    Protocol.Debugger.onPaused(() => {
      pausedCount++;
      InspectorTest.log('FAIL: unexpected Debugger.paused event');
    });

    await logResult(
        'pauseOnExceptions evaluate 42', await Protocol.Runtime.evaluate({
          expression: '42',
          awaitPromise: true,
          throwOnSideEffect: true,
          silent: false,
        }));

    await logResult(
        'pauseOnExceptions evaluate pending promise',
        await Protocol.Runtime.evaluate({
          expression: 'new Promise(() => {})',
          awaitPromise: true,
          throwOnSideEffect: true,
          silent: false,
        }));

    const {result: {result: {objectId}}} = await Protocol.Runtime.evaluate({
      expression: '({})',
    });

    await logResult(
        'pauseOnExceptions callFunctionOn 42',
        await Protocol.Runtime.callFunctionOn({
          objectId,
          functionDeclaration: 'function() { return 42; }',
          awaitPromise: true,
          throwOnSideEffect: true,
          silent: false,
        }));

    await logResult(
        'pauseOnExceptions callFunctionOn pending promise',
        await Protocol.Runtime.callFunctionOn({
          objectId,
          functionDeclaration: 'function() { return new Promise(() => {}); }',
          awaitPromise: true,
          throwOnSideEffect: true,
          silent: false,
        }));

    InspectorTest.log(`total paused events: ${pausedCount}`);

    await Protocol.Debugger.setPauseOnExceptions({state: 'none'});
    await Protocol.Debugger.disable();
  },
]);
