'use strict';

const common = require('../common');
const assert = require('assert');
const { EventEmitter } = require('events');
const { Writable } = require('stream');
const test = require('node:test');

for (const highWaterMark of [0, 1]) {
  for (const asynchronousDestroy of [false, true]) {
    for (const error of [undefined, new Error('premature native close')]) {
      test(`closing after premature destruction with highWaterMark ${highWaterMark}, ` +
           `asynchronousDestroy ${asynchronousDestroy}, error ${error !== undefined}`, async () => {
        const writable = new Writable({
          highWaterMark,
          write: common.mustCall(function(chunk, encoding, callback) {
            if (chunk[0] === 2) {
              this.destroy(error);
            }
            callback();
          }, 2),
          final: common.mustNotCall(),
          destroy: common.mustCall((reason, callback) => {
            if (asynchronousDestroy) {
              setImmediate(callback, reason);
            } else {
              callback(reason);
            }
          }),
        });
        writable.on('finish', common.mustNotCall());
        writable.on('close', common.mustCall());
        writable.on('error', error === undefined ?
          common.mustNotCall() :
          common.mustCall((actual) => assert.strictEqual(actual, error)));

        const writer = Writable.toWeb(writable).getWriter();
        await writer.write(Buffer.from([1]));
        const expectedError = error === undefined ?
          { code: 'ABORT_ERR' } :
          common.mustCall((actual) => actual === error, 2);
        const closed = assert.rejects(writer.closed, expectedError);
        await writer.write(Buffer.from([2]));
        assert.strictEqual(writable.destroyed, true);
        assert.strictEqual(writable.writableFinished, false);

        // Close before the native close event can report premature destruction.
        await Promise.all([
          assert.rejects(writer.close(), expectedError),
          closed,
        ]);
      });
    }
  }
}

for (const error of [false, true]) {
  test(`closing a prematurely destroyed legacy stream ignores boolean error ${error}`, async () => {
    const writable = new EventEmitter();
    writable.writable = true;
    writable.writableHighWaterMark = 16;
    writable._writableState = { errored: null, finished: false };
    writable.write = common.mustCall((chunk) => {
      if (chunk[0] === 2) {
        writable.destroyed = true;
        writable.writable = false;
        writable._writableState.errored = error;
        process.nextTick(() => writable.emit('close'));
      }
      return true;
    }, 2);
    writable.end = common.mustNotCall();
    writable.on('close', common.mustCall());

    const writer = Writable.toWeb(writable).getWriter();
    await writer.write(Buffer.from([1]));
    const closed = assert.rejects(writer.closed, { code: 'ABORT_ERR' });
    await writer.write(Buffer.from([2]));
    await Promise.all([
      assert.rejects(writer.close(), { code: 'ABORT_ERR' }),
      closed,
    ]);
  });
}

test('closing a normally finished native stream accepts autoDestroy', async () => {
  let writer;
  let closePromise;
  const writable = new Writable({
    autoDestroy: true,
    write: common.mustCall((chunk, encoding, callback) => callback()),
    final: common.mustCall((callback) => callback()),
    destroy: common.mustCall(function(error, callback) {
      assert.strictEqual(error, null);
      assert.strictEqual(this.destroyed, true);
      assert.strictEqual(this.writableFinished, true);
      closePromise = writer.close();
      callback(error);
    }),
  });
  writable.on('finish', common.mustCall());
  writable.on('close', common.mustCall());
  writable.on('error', common.mustNotCall());
  writer = Writable.toWeb(writable).getWriter();
  await writer.write(Buffer.from([1]));
  writable.end();
  await writer.closed;
  await closePromise;
});
