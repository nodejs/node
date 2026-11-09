// Flags: --experimental-web-worker
'use strict';

const common = require('../common');
const assert = require('node:assert');

// FileReaderSync stays off unless --experimental-fileapi is set, including
// inside a Web Worker. Worker setup runs after the global is removed.
const worker = new Worker(
  'data:text/javascript,postMessage(typeof FileReaderSync);close()',
);
worker.onmessage = common.mustCall(({ data }) => {
  assert.strictEqual(data, 'undefined');
});
