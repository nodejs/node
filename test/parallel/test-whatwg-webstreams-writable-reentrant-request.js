'use strict';
require('../common');
const assert = require('node:assert/strict');
const { test } = require('node:test');
const { promiseHooks } = require('node:v8');
const { WritableStream } = require('node:stream/web');

test('a write reentered during promise creation keeps its place in the queue', async () => {
  const received = [];
  const gate = Promise.withResolvers();
  const writer = new WritableStream({
    write(chunk) {
      received.push(chunk);
      if (chunk === 'nested') return gate.promise;
    },
  }).getWriter();
  await writer.ready;

  let firstPromise;
  let nested;
  const stop = promiseHooks.onInit((promise) => {
    if (firstPromise === undefined) {
      firstPromise = promise;
      nested = writer.write('nested');
    }
  });
  let outer;
  try {
    outer = writer.write('outer');
  } finally {
    stop();
  }
  assert.strictEqual(outer, firstPromise);
  assert.notStrictEqual(outer, nested);
  assert.deepStrictEqual(received, ['nested']);

  gate.resolve();
  await Promise.all([nested, outer]);
  assert.deepStrictEqual(received, ['nested', 'outer']);
  await writer.close();
});
