'use strict';

require('../common');
const assert = require('node:assert');
const { AsyncLocalStorage } = require('node:async_hooks');
const { test } = require('node:test');
const { setImmediate } = require('node:timers/promises');

// In the spec, a transform sink write that arrives while the stream has
// backpressure is a reaction to [[backpressureChangePromise]], and the
// writable controller reacts to the sink write promise. Both reactions are
// registered at write time, so transform() runs in the writer's async
// context, also when the reader that clears the backpressure runs in a
// different context.

function recordingTransform(als, seen) {
  return new TransformStream({
    transform(chunk, controller) {
      seen.push(`${chunk}:${als.getStore()}`);
      controller.enqueue(chunk);
    },
    flush() {
      seen.push(`flush:${als.getStore()}`);
    },
  });
}

test('pipeThrough() keeps the writer context after a parked write', async () => {
  const als = new AsyncLocalStorage();
  const seen = [];
  const readable = als.run('writer', () =>
    new ReadableStream({
      start(controller) {
        for (const chunk of 'abcde') controller.enqueue(chunk);
        controller.close();
      },
    }).pipeThrough(recordingTransform(als, seen)));

  // Let the internal pipe write the first chunk, which parks.
  await setImmediate();

  // Read outside als.run().
  await Array.fromAsync(readable);

  assert.deepStrictEqual(seen, [
    'a:writer', 'b:writer', 'c:writer', 'd:writer', 'e:writer', 'flush:writer',
  ]);
});

test('writer.write() keeps the writer context after a parked write', async () => {
  const als = new AsyncLocalStorage();
  const seen = [];
  const { readable, writable } = recordingTransform(als, seen);

  // Let start() settle, so that the first write parks on backpressure.
  await setImmediate();
  const done = als.run('writer', () => {
    const writer = writable.getWriter();
    return Promise.all([
      writer.write('a'),
      writer.write('b'),
      writer.write('c'),
      writer.close(),
    ]);
  });

  // Read outside als.run().
  await Array.fromAsync(readable);
  await done;

  assert.deepStrictEqual(seen, [
    'a:writer', 'b:writer', 'c:writer', 'flush:writer',
  ]);
});
