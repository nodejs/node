'use strict';

// A released BYOB reader's partially filled pull-into is moved to the
// queue on the next enqueue(). If a default reader is waiting with more
// than one read request, the queue must only fill as many read requests
// as it has chunks for, and the newly enqueued chunk must go to the next
// read request.

const common = require('../common');
const assert = require('assert');
const { ReadableStream } = require('stream/web');

async function test(numReads) {
  let controller;
  const rs = new ReadableStream({
    type: 'bytes',
    start(c) {
      controller = c;
    },
  });

  // Leave a pull-into with 1 of the required 2 bytes filled.
  const byobReader = rs.getReader({ mode: 'byob' });
  const byobRead = byobReader.read(new Uint8Array(2), { min: 2 });
  await new Promise(setImmediate);
  controller.enqueue(new Uint8Array([1]));
  byobReader.releaseLock();
  await assert.rejects(byobRead, { name: 'TypeError' });

  const reader = rs.getReader();
  const reads = [];
  for (let i = 0; i < numReads; i++) {
    reads.push(reader.read());
  }

  controller.enqueue(new Uint8Array([2]));
  controller.enqueue(new Uint8Array([3]));
  controller.close();

  const results = await Promise.all(reads);
  const expected = [[1], [2], [3]].slice(0, numReads);
  assert.deepStrictEqual(results.map(({ value }) => [...value]), expected);

  // Chunks that were not read yet are still in the queue.
  const rest = [];
  for (let result = await reader.read(); !result.done;
    result = await reader.read()) {
    rest.push(...result.value);
  }
  assert.deepStrictEqual(rest, [1, 2, 3].slice(numReads));
}

(async () => {
  for (const numReads of [1, 2, 3]) {
    await test(numReads);
  }
})().then(common.mustCall());
