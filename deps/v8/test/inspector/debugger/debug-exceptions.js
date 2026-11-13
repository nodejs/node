// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

let {session, contextGroup, Protocol} = InspectorTest.start(
    'Tests caught vs uncaught exception pause events with for-of and yield*.');

(async function test() {
  await Protocol.Debugger.enable();
  await Protocol.Runtime.enable();
  await Protocol.Debugger.setPauseOnExceptions({state: 'all'});

  const uncaughtList = [];
  Protocol.Debugger.onPaused(async ({params: {reason, data}}) => {
    if (reason === 'exception') {
      uncaughtList.push(data.uncaught);
    }
    await Protocol.Debugger.resume();
  });

  const donePromise = Protocol.Runtime.onceConsoleAPICalled();

  contextGroup.addScript(`
    function assertCaught(f) {
      try { f(); } finally {
        return;
      }
    }

    function assertUncaught(f) {
      try { f(); } finally {
        return;
      }
    }

    assertUncaught(() => {
      for (var a of [1, 2, 3]) {
        throw a;
      }
    });

    assertUncaught(() => {
      for (var a of [1, 2, 3]) {
        try { throw a; } finally {}
      }
    });

    assertCaught(() => {
      for (var a of [1, 2, 3]) {
        try {
          try { throw a; } finally {}
        } catch (_) {}
      }
    });

    assertCaught(() => {
      try {
        for (var a of [1, 2, 3]) {
          try { throw a; } finally {}
        }
      } catch (_) {}
    });

    // Check that an internal exception in the yield* desugaring is not
    // observable.
    {
      let iter = {
        next() { return {value: 42, done: false}; },
        throw() { return {done: true}; }
      };
      let iterable = {[Symbol.iterator]() { return iter; }};
      function* f() { yield* iterable; }

      let g = f();
      g.next();
      console.log(JSON.stringify(g.throw()));
    }
  `);

  const {params: {args}} = await donePromise;
  InspectorTest.log('uncaughtList: ' + JSON.stringify(uncaughtList));
  InspectorTest.log('yieldStarThrowResult: ' + args[0].value);

  InspectorTest.completeTest();
})();
