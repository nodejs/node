'use strict';
const common = require('../common');
const { Readable } = require('stream');
const assert = require('assert');
{
  const r = Readable.from([]);
  // Cancelling reader while closing should not cause uncaught exceptions
  r.on('close', common.mustCall(() => reader.cancel()));

  const reader = Readable.toWeb(r, { type: 'bytes' }).getReader({ mode: 'byob' });
  reader.read(new Uint8Array(16)).then(common.mustCall((result) => {
    assert.ok(result.done);
  }));
}
