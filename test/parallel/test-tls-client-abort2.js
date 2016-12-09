'use strict';
var common = require('../common');
var assert = require('assert');

if (!common.hasCrypto) {
  common.skip('missing crypto');
  return;
}
var tls = require('tls');

var conn = tls.connect(common.PORT, common.fail);
conn.on('error', common.mustCall(function() {
  assert.doesNotThrow(function() {
    conn.destroy();
  });
}));
