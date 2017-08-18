'use strict';
const common = require('../common');
if (!common.hasCrypto)
  common.skip('missing crypto');

const assert = require('assert');
const tls = require('tls');

function Done() {}

function test1() {
  let ciphers = '';

  tls.createSecureContext = function(options) {
    ciphers = options.ciphers;
    throw new Done();
  };

  assert.throws(tls.connect, Done);

  assert.strictEqual(ciphers, tls.DEFAULT_CIPHERS);
}
test1();
