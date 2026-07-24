// Flags: --experimental-stream-iter
'use strict';

const common = require('../common');
const assert = require('assert');
const { Writable } = require('stream');
const { fromWritable } = require('stream/iter');

{
  const writable = new Writable({ write() {} });

  fromWritable(writable);

  assert.throws(
    () => fromWritable(writable, { backpressure: 'invalid' }),
    { code: 'ERR_INVALID_ARG_VALUE' },
  );

  writable.destroy();
}

async function testCachedWritableUsesLaterBackpressureOptions() {
  const chunks = [];
  const writable = new Writable({
    highWaterMark: 1,
    write(chunk, encoding, callback) {
      chunks.push(Buffer.from(chunk));
    },
  });

  fromWritable(writable);
  const writer = fromWritable(writable, { backpressure: 'drop-newest' });

  await writer.write('a');
  await writer.write('b');

  assert.deepStrictEqual(
    chunks.map((chunk) => chunk.toString()),
    ['a'],
  );

  writable.destroy();
}

testCachedWritableUsesLaterBackpressureOptions().then(common.mustCall());
