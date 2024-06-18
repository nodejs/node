'use strict';

require('../common');
const { Readable, Writable } = require('stream');
const { finished } = require('stream/promises');
const { setImmediate } = require('node:timers/promises');
const assert = require('assert');

async function * asyncGenerator() {
  yield '1';
  await setImmediate();
  yield '2';
  await setImmediate();
  yield '3';
}

{
  async function test() {
    class NullWritable extends Writable {
      _write(c, e, cb) {
        cb();
      }
    }

    const src = Readable.from(asyncGenerator());
    const dest = new NullWritable();
    src.on('error', () => {});
    dest.on('error', () => {});

    src.pipe(dest);

    process.nextTick(() => {
      src.destroy(new Error('Error'));
    });

    await assert.rejects(finished(src), 'reject finish');

    assert.strictEqual(src.destroyed, true);
    assert.strictEqual(dest.destroyed, false);
    assert.strictEqual(src.closed, true);
    assert.strictEqual(dest.closed, false);
  }

  test();
}
