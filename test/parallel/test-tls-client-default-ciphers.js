'use strict';
const common = require('../common');
const assert = require('assert');

if (!common.hasCrypto) {
  common.skip('missing crypto');
  return;
}
const tls = require('tls');

function Done() {}

function test1() {
  let ciphers = '';

  tls.createSecureContext = function(options) {
    ciphers = options.ciphers;
    throw new Done();
  };

  try {
    tls.connect(common.PORT);
  } catch (e) {
    assert(e instanceof Done);
  }
  assert.strictEqual(ciphers, tls.DEFAULT_CIPHERS);
}
test1();
