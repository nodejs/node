'use strict';
const common = require('../common');
const fixtures = require('../common/fixtures');

if (!common.hasFipsCrypto)
  common.skip('node compiled without FIPS OpenSSL.');

const assert = require('assert');
const crypto = require('crypto');

const input = 'hello';

const dsapri = fixtures.readKey('dsa_private_1025.pem');
const sign = crypto.createSign('SHA1');
sign.update(input);

assert.throws(function() {
  sign.sign(dsapri);
}, /PEM_read_bio_PrivateKey failed/);
