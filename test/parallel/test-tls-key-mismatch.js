'use strict';
const common = require('../common');

if (!common.hasCrypto) {
  common.skip('missing crypto');
  return;
}
const assert = require('assert');
const tls = require('tls');
const fs = require('fs');
const errorMessageRegex = new RegExp('^Error: error:0B080074:x509 ' +
  'certificate routines:X509_check_private_key:key values mismatch$');

const options = {
  key: fs.readFileSync(common.fixturesDir + '/keys/agent1-key.pem'),
  cert: fs.readFileSync(common.fixturesDir + '/keys/agent2-cert.pem')
};

assert.throws(function() {
  tls.createSecureContext(options);
}, errorMessageRegex);
