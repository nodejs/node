'use strict';
const common = require('../common');
const assert = require('assert');

const { Readable } = require('stream');
const EE = require('events').EventEmitter;

class LegacyStream extends EE {
  pause() {}
  resume() {}
}

{
  const err = new Error();
  const oldStream = new LegacyStream();
  const r = new Readable({ autoDestroy: true })
    .wrap(oldStream)
    .on('error', common.mustCall(() => {
      assert.strictEqual(r._readableState.errorEmitted, true);
      assert.strictEqual(r._readableState.errored, err);
      assert.strictEqual(r.destroyed, true);
    }));
  oldStream.emit('error', err);
}

{
  const err = new Error();
  const oldStream = new LegacyStream();
  const r = new Readable({ autoDestroy: false })
    .wrap(oldStream)
    .on('error', common.mustCall(() => {
      assert.strictEqual(r._readableState.errorEmitted, true);
      assert.strictEqual(r._readableState.errored, err);
      assert.strictEqual(r.destroyed, false);
    }));
  oldStream.emit('error', err);
}
