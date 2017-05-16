'use strict';

const common = require('../common');

// Assert that the zlib output on MacOS and Linux is identical to what it was
// before the zlib 1.2.11 update. The change was in an unused 'OS code' in the
// header, so is not dependent on the size or content of the data encrypted.
// The test doesn't apply to non-MacOS/Linux platforms, they already had unique
// OS codes prior to the zlib update.

if (process.platform !== 'linux' && process.platform !== 'darwin')
  return common.skip('test applies only to Mac and Linux');

const assert = require('assert');
const zlib = require('zlib');

const expected = '1f8b0800000000000003ab00008316dc8c01000000';
const options = {strategy: zlib.Z_DEFAULT_STRATEGY};

zlib.gzip('x', options, common.mustCall(function(err, compressed) {
  assert.strictEqual(compressed.toString('hex'), expected);
}));
