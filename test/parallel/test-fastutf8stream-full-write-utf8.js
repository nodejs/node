'use strict';

// Regression test: after a multi-byte UTF-8 chunk is *fully* written, the
// stream must keep flushing the remaining buffered chunks instead of stalling.
//
// `releaseWritingBuf()` tracks the buffered length in characters, but on a full
// write it used to subtract the number of *bytes* reported by fs.write instead
// of the number of *characters*. For multi-byte data this drove the internal
// length to zero, so the stream emitted 'drain' and went idle while queued
// chunks were left unwritten.

const common = require('../common');
const assert = require('node:assert');
const { Utf8Stream } = require('node:fs');

// "€" is a single JS character that encodes to three UTF-8 bytes, so the byte
// count and character count differ - which is exactly what triggered the bug.
const CHAR = '€';
const COUNT = 3;

const chunks = [];
const fsOverride = {
  // Always report a full (successful) write.
  write: common.mustCallAtLeast((fd, data, enc, cb) => {
    chunks.push(data);
    process.nextTick(cb, null, Buffer.byteLength(data));
  }, COUNT),
  writeSync() { throw new Error('writeSync should not be used in async mode'); },
  fsync(fd, cb) { cb(); },
  fsyncSync() {},
  close(fd, cb) { cb(); },
  open(path, flags, mode, cb) { cb(null, 42); },
  mkdir(path, opts, cb) { cb(); },
  mkdirSync() {},
};

const stream = new Utf8Stream({
  fd: 42,
  sync: false,
  minLength: 0,
  // Force each character into its own buffered chunk so that, while the first
  // write is in flight, the remaining characters stay queued.
  maxWrite: 1,
  fs: fsOverride,
});

stream.on('ready', common.mustCall(() => {
  for (let i = 0; i < COUNT; i++) {
    stream.write(CHAR);
  }

  // Without calling end(): the stream must flush everything on its own.
  setTimeout(common.mustCall(() => {
    assert.strictEqual(chunks.length, COUNT,
                       `expected ${COUNT} writes, got ${chunks.length}`);
    assert.strictEqual(chunks.join(''), CHAR.repeat(COUNT));
    stream.destroy();
  }), common.platformTimeout(100));
}));
