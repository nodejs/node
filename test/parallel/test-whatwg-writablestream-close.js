'use strict';

require('../common');
const { test } = require('node:test');
const assert = require('node:assert');

// https://github.com/nodejs/node/issues/57272

test('should throw error when writing after close', async (t) => {
  const writable = new WritableStream({
    write(chunk) {
      console.log(chunk);
    },
  });

  const writer = writable.getWriter();

  await writer.write('Hello');
  await writer.close();

  await assert.rejects(
    async () => {
      await writer.write('World');
    },
    {
      name: 'TypeError',
    }
  );
});

test('should reject writes while the close is in flight', async () => {
  const { promise, resolve } = Promise.withResolvers();
  const writable = new WritableStream({
    close() {
      return promise;
    },
  });

  const writer = writable.getWriter();
  const closed = writer.close();
  // Let the close request reach the sink.
  await new Promise(setImmediate);

  await assert.rejects(writer.write('World'), { name: 'TypeError' });

  resolve();
  await closed;
});

test('should reject writes to a TransformStream while flush() is pending', async () => {
  const { promise, resolve } = Promise.withResolvers();
  const transform = new TransformStream({
    flush() {
      return promise;
    },
  });

  const writer = transform.writable.getWriter();
  const closed = writer.close();
  await new Promise(setImmediate);

  await assert.rejects(writer.write('World'), { name: 'TypeError' });

  resolve();
  await closed;
});
