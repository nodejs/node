'use strict';
const common = require('../common');
const assert = require('assert');

if (!common.hasCrypto) {
  common.skip('missing crypto');
  return;
}
const tls = require('tls');

const conn = tls.connect(common.PORT, common.mustNotCall());
conn.on('error', common.mustCall(function() {
  assert.doesNotThrow(function() {
    conn.destroy();
  });
}));
