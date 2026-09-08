'use strict';
const assert = require('node:assert');
const { once } = require('node:events');
const { text } = require('node:stream/consumers');
const { testCreatePipe } = require('../common/net-create-pipe');

async function raceFinishBeforeRead(readable, writable) {
  const finish = once(writable, 'finish').then(() => 'finish');
  const tick = new Promise((resolve) => setImmediate(resolve, 'pending'));
  writable.end('abc');
  const result = await Promise.race([finish, tick]);

  readable.resume();
  await text(readable);
  await finish;
  return result;
}

if (process.platform !== 'win32') {
  testCreatePipe('net pipe writer can finish before pipe reader consumes',
    async (readable, writable) => {
      assert.strictEqual(
        await raceFinishBeforeRead(readable, writable),
        'finish');
    });
}

if (process.platform === 'win32') {
  testCreatePipe('net pipe writer finish waits until pipe reader consumes',
    async (readable, writable) => {
      assert.strictEqual(
        await raceFinishBeforeRead(readable, writable),
        'pending');
    });
}

// A possible hack to unify this behavior would be to bypass shutdown for
// parent-owned net pipe writers would be to add the following to
// Socket.prototype._final:
//
// if (this[kWriterOfPair]) {
//   debug('_final: pipe pair writable, close handle');
//   process.nextTick(() => this.destroy());
//   return cb();
// }
