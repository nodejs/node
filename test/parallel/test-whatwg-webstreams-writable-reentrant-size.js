'use strict';
require('../common');
const assert = require('node:assert/strict');
const { WritableStream } = require('node:stream/web');
const { test } = require('node:test');

test('a reentrant write counts the active chunk until it settles', async () => {
  const gate = Promise.withResolvers();
  const observed = [];
  let nested;
  const writer = new WritableStream({
    write(size) {
      observed.push([size, writer.desiredSize]);
      if (size === 2) {
        nested = writer.write(3);
        assert.strictEqual(writer.desiredSize, 1);
        return gate.promise;
      }
    },
  }, { highWaterMark: 6, size: (size) => size }).getWriter();

  await writer.ready;
  const first = writer.write(2);
  const last = writer.write(1);
  assert.deepStrictEqual(observed, [[2, 4]]);
  assert.strictEqual(writer.desiredSize, 0);

  gate.resolve();
  await Promise.all([first, nested, last]);
  assert.deepStrictEqual(observed, [[2, 4], [3, 2], [1, 5]]);
  assert.strictEqual(writer.desiredSize, 6);
  await writer.close();
});
