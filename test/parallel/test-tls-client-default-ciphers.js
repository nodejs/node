'use strict';
var assert = require('assert');
var common = require('../common');

if (!common.hasCrypto) {
  common.skip('missing crypto');
  return;
}
var tls = require('tls');

function Done() {}

function test1() {
  var ciphers = '';

  tls.createSecureContext = function(options) {
    ciphers = options.ciphers;
    throw new Done();
  };

  try {
    tls.connect(common.PORT);
  } catch (e) {
    assert(e instanceof Done);
  }
  assert.equal(ciphers, tls.DEFAULT_CIPHERS);
}
test1();
