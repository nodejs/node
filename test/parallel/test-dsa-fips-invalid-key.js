'use strict';
const common = require('../common');
if (!common.hasFipsCrypto)
  common.skip('node compiled without FIPS OpenSSL.');

const assert = require('assert');
const crypto = require('crypto');
const fs = require('fs');

const input = 'hello';

const dsapri = fs.readFileSync(
  `${common.fixturesDir}/keys/dsa_private_1025.pem`);
const sign = crypto.createSign('DSS1');
sign.update(input);

assert.throws(function() {
  sign.sign(dsapri);
}, /PEM_read_bio_PrivateKey failed/);
