'use strict';
const common = require('../common');

if (!common.hasCrypto)
  common.skip('missing crypto');

const fixtures = require('../common/fixtures');

const assert = require('assert');
const tls = require('tls');
const errorMessageRegex =
  /^Error: error:0B080074:x509 certificate routines:X509_check_private_key:key values mismatch$/;

const options = {
  key: fixtures.readKey('agent1-key.pem'),
  cert: fixtures.readKey('agent2-cert.pem')
};

assert.throws(function() {
  tls.createSecureContext(options);
}, errorMessageRegex);
