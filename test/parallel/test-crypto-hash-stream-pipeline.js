'use strict';

const common = require('../common');

if (!common.hasCrypto)
  common.skip('missing crypto');

const assert = require('assert');
const crypto = require('crypto');
const stream = require('stream');

const hash = crypto.createHash('md5');
const s = new stream.PassThrough();
const expect = 'e8dc4081b13434b45189a720b77b6818';

s.write('abcdefgh');
stream.pipeline(s, hash, common.mustCall(function(err) {
  assert.ifError(err);
  assert.strictEqual(hash.digest('hex'), expect);
}));
s.end();
