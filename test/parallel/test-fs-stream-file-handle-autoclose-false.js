'use strict';

// Regression test for https://github.com/nodejs/node/issues/45721
// Destroying a stream created from a FileHandle with `autoClose: false` must
// not close the FileHandle, and must not leave a 'close' listener behind on
// the FileHandle.

const common = require('../common');
const assert = require('assert');
const fs = require('fs');
const tmpdir = require('../common/tmpdir');

tmpdir.refresh();
const file = tmpdir.resolve('stream-file-handle-autoclose-false.txt');
const input = 'x'.repeat(1024);
fs.writeFileSync(file, input);

(async () => {
  // Partially consuming a ReadStream with `break` destroys it.
  const handle = await fs.promises.open(file, 'r');
  handle.on('close', common.mustCall());
  const listeners = handle.listenerCount('close');

  // Run more iterations than the default maxListeners (10) to make sure
  // no listener leaks onto the long-lived FileHandle.
  process.on('warning', common.mustNotCall());
  for (let i = 0; i < 15; i++) {
    const stream = handle.createReadStream({
      autoClose: false,
      start: 0,
      highWaterMark: 16,
    });
    for await (const chunk of stream) {
      assert.strictEqual(chunk.toString(), input.slice(0, 16));
      break;
    }
    assert.strictEqual(stream.destroyed, true);
    // The stream is only closed once any pending read has completed.
    if (!stream.closed) {
      await new Promise((resolve) => stream.once('close', resolve));
    }
    assert.notStrictEqual(handle.fd, -1);
    assert.strictEqual(handle.listenerCount('close'), listeners);
  }

  // Explicitly destroying the stream.
  const stream = handle.createReadStream({ autoClose: false, start: 0 });
  stream.destroy();
  await new Promise((resolve) => stream.on('close', resolve));
  assert.notStrictEqual(handle.fd, -1);

  // The FileHandle is still usable.
  const { bytesRead } = await handle.read(Buffer.alloc(4), 0, 4, 0);
  assert.strictEqual(bytesRead, 4);
  await handle.close();
  assert.strictEqual(handle.fd, -1);
})().then(common.mustCall());

(async () => {
  // Same for WriteStream.
  const handle = await fs.promises.open(tmpdir.resolve('ws.txt'), 'w');
  handle.on('close', common.mustCall());
  const stream = handle.createWriteStream({ autoClose: false });
  stream.write('hello');
  stream.destroy();
  await new Promise((resolve) => stream.on('close', resolve));
  assert.notStrictEqual(handle.fd, -1);
  assert.strictEqual(handle.listenerCount('close'), 1);
  await handle.write('world');
  await handle.close();
})().then(common.mustCall());

(async () => {
  // Closing the FileHandle while a stream with `autoClose: false` holds a
  // reference to it still closes the stream and the FileHandle.
  const handle = await fs.promises.open(file, 'r');
  const stream = handle.createReadStream({ autoClose: false });
  stream.on('close', common.mustCall());
  await handle.close();
  assert.strictEqual(handle.fd, -1);
})().then(common.mustCall());

(async () => {
  // `autoClose: true` (the default) still closes the FileHandle on destroy.
  const handle = await fs.promises.open(file, 'r');
  handle.on('close', common.mustCall());
  const stream = handle.createReadStream();
  stream.destroy();
  await new Promise((resolve) => stream.on('close', resolve));
  assert.strictEqual(handle.fd, -1);
  assert.strictEqual(handle.listenerCount('close'), 1);
})().then(common.mustCall());
