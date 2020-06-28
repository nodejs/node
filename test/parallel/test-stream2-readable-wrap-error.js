'use strict';
const common = require('../common');
const assert = require('assert');

const Readable = require('_stream_readable');
const EE = require('events').EventEmitter;

const oldStream = new EE();
oldStream.pause = () => {};
oldStream.resume = () => {};

{
  const r = new Readable({ autoDestroy: true })
    .wrap(oldStream)
    .on('error', common.mustCall(() => {
      assert.strictEqual(r._readableState.errorEmitted, true);
      assert.strictEqual(r._readableState.errored, true);
      assert.strictEqual(r.destroyed, true);
    }));
  oldStream.emit('error', new Error());
}

{
  const r = new Readable({ autoDestroy: false })
    .wrap(oldStream)
    .on('error', common.mustCall(() => {
      assert.strictEqual(r._readableState.errorEmitted, true);
      assert.strictEqual(r._readableState.errored, true);
      assert.strictEqual(r.destroyed, false);
    }));
  oldStream.emit('error', new Error());
}
