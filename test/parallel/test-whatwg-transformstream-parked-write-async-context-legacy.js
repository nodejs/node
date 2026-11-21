// Flags: --no-async-context-frame
'use strict';

// A write parked on backpressure must run transform() and the writes queued
// behind it in the async context of the writer, not in the context of the
// read that lifts the backpressure.
// This variant covers the async_hooks-based AsyncLocalStorage.
// Refs: https://github.com/nodejs/node/issues/66906

const common = require('../common');
const assert = require('assert');
const { AsyncLocalStorage } = require('async_hooks');
const { setImmediate } = require('timers/promises');

const als = new AsyncLocalStorage();

function createPipeline(seen) {
  return new ReadableStream({
    start(controller) {
      for (const chunk of 'abc') controller.enqueue(chunk);
      controller.close();
    },
  }).pipeThrough(new TransformStream({
    transform(chunk, controller) {
      seen.push(`${chunk}:${als.getStore() ?? 'none'}`);
      controller.enqueue(chunk);
    },
    flush() {
      seen.push(`flush:${als.getStore() ?? 'none'}`);
    },
  }));
}

async function consume(readable) {
  // eslint-disable-next-line no-unused-vars
  for await (const _ of readable);
}

(async () => {
  {
    const seen = [];
    const readable = als.run('writer', () => createPipeline(seen));
    // Let the pipe park its first write on backpressure.
    await setImmediate();
    await consume(readable);
    assert.deepStrictEqual(seen, [
      'a:writer', 'b:writer', 'c:writer', 'flush:writer',
    ]);
  }

  {
    const seen = [];
    const readable = createPipeline(seen);
    await setImmediate();
    await als.run('reader', () => consume(readable));
    assert.deepStrictEqual(seen, [
      'a:none', 'b:none', 'c:none', 'flush:none',
    ]);
  }
})().then(common.mustCall());
