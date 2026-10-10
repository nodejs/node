'use strict';

const common = require('../common');
const assert = require('assert');
const { setImmediate: setImmediatePromise } = require('timers/promises');

// The tee's cancel promise is materialized by the first branch cancel and
// its error watcher is installed as the reader's closed record. Check
// that they settle the same way whether the source closes or errors
// before or after, for default and byte streams.

function makeSource(type, onCancel) {
  let controller;
  const stream = new ReadableStream({
    type,
    start(c) { controller = c; },
    cancel: onCancel,
  });
  return { stream, controller };
}

// A branch cancel promise settles from the tee's close steps, which run
// when a read is in flight, or once both branches are canceled.
async function cancelOneThenClose(type) {
  const { stream, controller } = makeSource(type, common.mustNotCall());
  const [branch1, branch2] = stream.tee();
  const cancelPromise = branch1.cancel('one');
  const readPromise = branch2.getReader().read();
  await setImmediatePromise();
  controller.close();
  assert.strictEqual(await cancelPromise, undefined);
  const { done } = await readPromise;
  assert.strictEqual(done, true);
}

async function closeThenCancelBoth(type) {
  const { stream, controller } = makeSource(type, common.mustNotCall());
  const [branch1, branch2] = stream.tee();
  controller.close();
  await setImmediatePromise();
  const cancel1 = branch1.cancel('one');
  const cancel2 = branch2.cancel('two');
  assert.strictEqual(await cancel1, undefined);
  assert.strictEqual(await cancel2, undefined);
}

async function cancelOneThenError(type) {
  const { stream, controller } = makeSource(type, common.mustNotCall());
  const [branch1, branch2] = stream.tee();
  const cancelPromise = branch1.cancel('one');
  await setImmediatePromise();
  const error = new Error('boom');
  controller.error(error);
  assert.strictEqual(await cancelPromise, undefined);
  await assert.rejects(branch2.getReader().read(), error);
}

async function cancelBoth(type) {
  const { stream } = makeSource(type, common.mustCall((reason) => {
    assert.deepStrictEqual(reason, ['one', 'two']);
  }));
  const [branch1, branch2] = stream.tee();
  const cancel1 = branch1.cancel('one');
  await setImmediatePromise();
  const cancel2 = branch2.cancel('two');
  assert.strictEqual(await cancel1, undefined);
  assert.strictEqual(await cancel2, undefined);
}

async function teeErroredSource(type) {
  const { stream, controller } = makeSource(type, common.mustNotCall());
  const error = new Error('boom');
  controller.error(error);
  const [branch1, branch2] = stream.tee();
  const reader1 = branch1.getReader();
  await assert.rejects(reader1.read(), error);
  await assert.rejects(branch2.getReader().read(), error);
  await assert.rejects(reader1.cancel('one'), error);
}

(async () => {
  for (const type of [undefined, 'bytes']) {
    await teeErroredSource(type);
    await cancelOneThenClose(type);
    await closeThenCancelBoth(type);
    await cancelOneThenError(type);
    await cancelBoth(type);
  }
})().then(common.mustCall());
