'use strict';

const common = require('../common');

const assert = require('assert');
const zlib = require('zlib');

// Verify that the zlib transform does clean up
// the handle when calling destroy.

{
  const ts = zlib.createGzip();
  ts.destroy();
  assert.strictEqual(ts._handle, null);

  ts.on('close', common.mustCall(() => {
    ts.close(common.mustCall());
  }));
}

{
  // Ensure 'error' is only emitted once.
  const decompress = zlib.createGunzip(15);

  decompress.on('error', common.mustCall((err) => {
    decompress.close();
  }));

  decompress.write('something invalid');
  decompress.destroy(new Error('asd'));
}
