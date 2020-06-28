'use strict';
const common = require('../common');
const assert = require('assert');

const Readable = require('_stream_readable');
const EE = require('events').EventEmitter;

class LegacyStream extends EE {
  pause() {}
  resume() {}
}

{
  const oldStream = new LegacyStream();
  const r = new Readable({ autoDestroy: true })
    .wrap(oldStream)
    .on('error', common.mustCall(() => {
      assert.strictEqual(r.destroyed, true);
    }));
  oldStream.emit('error', new Error());
}

{
  const oldStream = new LegacyStream();
  const r = new Readable({ autoDestroy: false })
    .wrap(oldStream)
    .on('error', common.mustCall(() => {
      assert.strictEqual(r.destroyed, false);
    }));
  oldStream.emit('error', new Error());
}
