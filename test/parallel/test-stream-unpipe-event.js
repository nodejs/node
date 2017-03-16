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

const dest1 = new NullWriteable();
dest1.on('pipe', common.mustCall(() => {}));
dest1.on('unpipe', common.mustCall(() => {}));
const src1 = new QuickEndReadable();
src1.pipe(dest1);


const dest2 = new NullWriteable();
dest2.on('pipe', common.mustCall(() => {}));
dest2.on('unpipe', () => {
  throw new Error('unpipe should not have been emited');
});
const src2 = new NeverEndReadable();
src2.pipe(dest2);

const dest3 = new NullWriteable();
dest3.on('pipe', common.mustCall(() => {}));
dest3.on('unpipe', common.mustCall(() => {}));
const src3 = new NeverEndReadable();
src3.pipe(dest3);
src3.unpipe(dest3);

const dest4 = new NullWriteable();
dest4.on('pipe', common.mustCall(() => {}));
dest4.on('unpipe', common.mustCall(() => {}));
const src4 = new QuickEndReadable();
src4.pipe(dest4, {end: false});

const dest5 = new NullWriteable();
dest5.on('pipe', common.mustCall(() => {}));
dest5.on('unpipe', () => {
  throw new Error('unpipe should not have been emited');
});
const src5 = new NeverEndReadable();
src5.pipe(dest5, {end: false});

const dest6 = new NullWriteable();
dest6.on('pipe', common.mustCall(() => {}));
dest6.on('unpipe', common.mustCall(() => {}));
const src6 = new NeverEndReadable();
src6.pipe(dest6, {end: false});
src6.unpipe(dest6);

setImmediate(() => {
  assert.strictEqual(src1._readableState.pipesCount, 0);
  assert.strictEqual(src2._readableState.pipesCount, 1);
  assert.strictEqual(src3._readableState.pipesCount, 0);
  assert.strictEqual(src4._readableState.pipesCount, 0);
  assert.strictEqual(src5._readableState.pipesCount, 1);
  assert.strictEqual(src6._readableState.pipesCount, 0);
});
