// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// InjectedScript::getInternalAndPrivateProperties() must stop calling into V8
// once mirroring an internal property has terminated execution. Otherwise the
// pending termination is cleared by the next V8 API call, another user getter
// runs, queued microtasks are executed, and the truncated result is reported
// as a success.

const {session, contextGroup, Protocol} = InspectorTest.start(
    'Tests that termination while mirroring an internal property stops the ' +
    'private property mirroring in Runtime.getProperties.');

const setup = `
  var log = [];
  var pausing = false;
  function makeError(tag, terminate) {
    const err = new Error();
    Object.setPrototypeOf(err, {
      get name() {
        // Formatting err1 again while paused must not re-enter the loop.
        if (pausing && terminate) return 'Error';
        log.push(tag);
        if (terminate) {
          pausing = true;
          Promise.resolve().then(() => log.push('microtask'));
          debugger;
          for (;;) {}
        }
        return tag;
      },
    });
    return err;
  }
  globalThis.err1 = makeError('err1', true);
  globalThis.err2 = makeError('err2', false);
  class A { constructor(o) { return o; } }
  class B extends A {
    #f;
    constructor(o, v) { super(o); this.#f = v; }
  }
  // p has err1 as its [[PromiseResult]] internal property and err2 as the
  // value of its private field #f.
  globalThis.p = Promise.resolve(globalThis.err1);
  new B(globalThis.p, globalThis.err2);
  42;
`;

(async () => {
  await Protocol.Runtime.enable();
  await Protocol.Debugger.enable();
  await Protocol.Runtime.evaluate({expression: setup});

  const {result: {result: p}} =
      await Protocol.Runtime.evaluate({expression: 'globalThis.p'});

  const paused = Protocol.Debugger.oncePaused();
  const properties = Protocol.Runtime.getProperties({objectId: p.objectId});
  await paused;
  Protocol.Runtime.terminateExecution();
  await Protocol.Debugger.resume();
  const response = await properties;
  InspectorTest.log(
      'getProperties after terminateExecution: ' +
      (response.error ? response.error.message : 'NO ERROR'));

  const {result: {result}} =
      await Protocol.Runtime.evaluate({expression: 'JSON.stringify(log)'});
  InspectorTest.log('ran after termination was requested: ' + result.value);

  InspectorTest.completeTest();
})();
