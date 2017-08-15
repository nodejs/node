'use strict';
const common = require('../common');
if (!common.hasCrypto)
  common.skip('missing crypto');

const assert = require('assert');
const tls = require('tls');

const conn = tls.connect(0, common.mustNotCall());
conn.on('error', common.mustCall(function() {
  assert.doesNotThrow(function() {
    conn.destroy();
  });
}));
