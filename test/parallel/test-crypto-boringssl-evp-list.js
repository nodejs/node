'use strict';

const common = require('../common');
if (!common.hasCrypto)
  common.skip('missing crypto');

if (!process.features.openssl_is_boringssl)
  common.skip('BoringSSL-only test');

const assert = require('assert');
const { getCiphers, getHashes } = require('crypto');

const ciphers = getCiphers();
[
  'aes-128-cbc',
  'aes-256-gcm',
  'des-ede',
  'des-ede-cbc',
  'des-ede3-cbc',
  'rc2-cbc',
  'rc4',
].forEach((cipher) => assert(ciphers.includes(cipher), cipher));

const hashes = getHashes();
[
  'md4',
  'md5',
  'sha1',
  'sha256',
  'sha512-256',
].forEach((hash) => assert(hashes.includes(hash), hash));
