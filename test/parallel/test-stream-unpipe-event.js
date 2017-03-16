'use strict';
const common = require('../common');
const assert = require('assert');
const {Writable, Readable} = require('stream');
class NullWriteable extends Writable {
  _write(chunk, encoding, callback) {
    return callback();
  }
}
class QuickEndReadable extends Readable {
  _read() {
    this.push(null);
  }
}
class NeverEndReadable extends Readable {
  _read() {}
}

function noop() {}

{
  const dest = new NullWriteable();
  const src = new QuickEndReadable();
  dest.on('pipe', common.mustCall(noop));
  dest.on('unpipe', common.mustCall(noop));
  src.pipe(dest);
  setImmediate(() => {
    assert.strictEqual(src._readableState.pipesCount, 0);
  });
}

{
  const dest = new NullWriteable();
  const src = new NeverEndReadable();
  dest.on('pipe', common.mustCall(noop));
  dest.on('unpipe', common.mustNotCall('unpipe should not have been emitted'));
  src.pipe(dest);
  setImmediate(() => {
    assert.strictEqual(src._readableState.pipesCount, 1);
  });
}

{
  const dest = new NullWriteable();
  const src = new NeverEndReadable();
  dest.on('pipe', common.mustCall(noop));
  dest.on('unpipe', common.mustCall(noop));
  src.pipe(dest);
  src.unpipe(dest);
  setImmediate(() => {
    assert.strictEqual(src._readableState.pipesCount, 0);
  });
}

{
  const dest = new NullWriteable();
  const src = new QuickEndReadable();
  dest.on('pipe', common.mustCall(noop));
  dest.on('unpipe', common.mustCall(noop));
  src.pipe(dest, {end: false});
  setImmediate(() => {
    assert.strictEqual(src._readableState.pipesCount, 0);
  });
}

{
  const dest = new NullWriteable();
  const src = new NeverEndReadable();
  dest.on('pipe', common.mustCall(noop));
  dest.on('unpipe', common.mustNotCall('unpipe should not have been emitted'));
  src.pipe(dest, {end: false});
  setImmediate(() => {
    assert.strictEqual(src._readableState.pipesCount, 1);
  });
}

{
  const dest = new NullWriteable();
  const src = new NeverEndReadable();
  dest.on('pipe', common.mustCall(noop));
  dest.on('unpipe', common.mustCall(noop));
  src.pipe(dest, {end: false});
  src.unpipe(dest);
  setImmediate(() => {
    assert.strictEqual(src._readableState.pipesCount, 0);
  });
}
