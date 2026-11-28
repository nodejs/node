'use strict';
const common = require('../common');
const assert = require('assert');
const child_process = require('child_process');

// Messages of varied sizes, many larger than one IPC read, carrying typed
// arrays of several element sizes. Frames that span reads are reassembled
// natively; check that their contents arrive intact and in order.
const sizes = [1, 7, 4096, 65535, 65536, 65537, 200003, 3, 1 << 20, 13];

function makeMessage(i, size) {
  const bytes = Buffer.alloc(size);
  for (let j = 0; j < size; j++) bytes[j] = (i * 31 + j) & 0xff;
  const doubles = new Float64Array(size >>> 3 || 1);
  for (let j = 0; j < doubles.length; j++) doubles[j] = i + j / 8;
  const shorts = new Uint16Array(size >>> 1 || 1);
  for (let j = 0; j < shorts.length; j++) shorts[j] = (i + j) & 0xffff;
  return { i, bytes, doubles, shorts };
}

if (process.argv[2] === 'child') {
  let i = 0;
  (function send() {
    if (i < sizes.length) {
      process.send(makeMessage(i, sizes[i]), send);
      i++;
    } else {
      process.disconnect();
    }
  })();
} else {
  const child = child_process.spawn(process.execPath, [__filename, 'child'], {
    stdio: ['inherit', 'inherit', 'inherit', 'ipc'],
    serialization: 'advanced',
  });
  let next = 0;
  child.on('message', common.mustCall((message) => {
    const expected = makeMessage(next, sizes[next]);
    assert.strictEqual(message.i, next);
    assert.ok(Buffer.isBuffer(message.bytes));
    assert.deepStrictEqual(message.bytes, expected.bytes);
    assert.deepStrictEqual(message.doubles, expected.doubles);
    assert.deepStrictEqual(message.shorts, expected.shorts);
    next++;
  }, sizes.length));
  child.on('exit', common.mustCall(() => {
    assert.strictEqual(next, sizes.length);
  }));
}
