'use strict';
var common = require('../common');
var assert = require('assert');

if (!common.hasCrypto) {
  console.log('1..0 # Skipped: missing crypto');
  return;
}

var crypto = require('crypto');
var fs = require('fs');

if (!crypto.hasFipsCrypto()) {
  console.log('1..0 # Skipped: node started without FIPS OpenSSL.');
  return;
}

var input = 'hello';

var dsapri = fs.readFileSync(common.fixturesDir +
                             '/keys/dsa_private_1025.pem');
var sign = crypto.createSign('DSS1');
sign.update(input);

assert.throws(function() {
  sign.sign(dsapri);
}, /PEM_read_bio_PrivateKey failed/);
