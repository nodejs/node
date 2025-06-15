'use strict';
require('../common');
const { Readable } = require('stream');
{
  const r = Readable.from([]);
  // Cancelling reader while closing should not cause uncaught exceptions
  r.on('close', () => reader.cancel());

  const reader = Readable.toWeb(r, { type: 'bytes' }).getReader({ mode: 'byob' });
  reader.read(new Uint8Array(16));
}
