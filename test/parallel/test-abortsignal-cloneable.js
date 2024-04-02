'use strict';

const common = require('../common');
const { ok, strictEqual } = require('assert');
const { setImmediate: pause } = require('timers/promises');
const {
  transferableAbortSignal,
  transferableAbortController,
} = require('util');


function deferred() {
  let res;
  const promise = new Promise((resolve) => res = resolve);
  return { res, promise };
}

(async () => {
  const ac = transferableAbortController();
  const mc = new MessageChannel();

  const deferred1 = deferred();
  const deferred2 = deferred();
  const resolvers = [deferred1, deferred2];

  mc.port1.onmessage = common.mustCall(({ data }) => {
    data.addEventListener('abort', common.mustCall(() => {
      strictEqual(data.reason, 'boom');
    }));
    resolvers.shift().res();
  }, 2);

  mc.port2.postMessage(ac.signal, [ac.signal]);

  // Can be cloned/transferd multiple times and they all still work
  mc.port2.postMessage(ac.signal, [ac.signal]);

  mc.port2.close();

  // Although we're using transfer semantics, the local AbortSignal
  // is still usable locally.
  ac.signal.addEventListener('abort', common.mustCall(() => {
    strictEqual(ac.signal.reason, 'boom');
  }));

  await Promise.all([ deferred1.promise, deferred2.promise ]);

  ac.abort('boom');

  // Because the postMessage used by the underlying AbortSignal
  // takes at least one turn of the event loop to be processed,
  // and because it is unref'd, it won't, by itself, keep the
  // event loop open long enough for the test to complete, so
  // we schedule two back to back turns of the event to ensure
  // the loop runs long enough for the test to complete.
  await pause();
  await pause();

})().then(common.mustCall());

{
  const signal = transferableAbortSignal(AbortSignal.abort('boom'));
  ok(signal.aborted);
  strictEqual(signal.reason, 'boom');
  const mc = new MessageChannel();
  mc.port1.onmessage = common.mustCall(({ data }) => {
    ok(data instanceof AbortSignal);
    ok(data.aborted);
    strictEqual(data.reason, 'boom');
    mc.port1.close();
  });
  mc.port2.postMessage(signal, [signal]);
}

{
  // The cloned AbortSignal does not keep the event loop open
  // waiting for the abort to be triggered.
  const ac = transferableAbortController();
  const mc = new MessageChannel();
  mc.port1.onmessage = common.mustCall();
  mc.port2.postMessage(ac.signal, [ac.signal]);
  mc.port2.close();
}
