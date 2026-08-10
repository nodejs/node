// Flags: --expose-internals
'use strict';
const common = require('../common');
if (!common.hasCrypto) common.skip('missing crypto');

if (process.features.openssl_is_boringssl)
  common.skip('BoringSSL does not support FIPS');

const { internalBinding } = require('internal/test/binding');
const { testFipsCrypto } = internalBinding('crypto');
if (!testFipsCrypto()) common.skip('no FIPS provider available');

const assert = require('assert');
const crypto = require('crypto');

// Also covers --force-fips, which makes setFips() throw.
if (crypto.getFips()) common.skip('FIPS is already enabled');

if (!crypto.getCiphers().includes('sm4-cbc'))
  common.skip('SM4 support is disabled in this build');

// Provider-only ciphers are fetched once and cached. Enabling FIPS changes the
// default properties every fetch is resolved against, so the cached instance
// must not survive the switch: SM4 is not FIPS-approved.
// Refs: https://github.com/nodejs/node/issues/64866

const key = Buffer.alloc(16);
const iv = Buffer.alloc(12);

// Populate the cache while FIPS is still disabled.
assert(crypto.getCiphers().includes('sm4-gcm'));
crypto.createCipheriv('sm4-gcm', key, iv);

crypto.setFips(true);
assert.strictEqual(crypto.getFips(), 1);

assert(!crypto.getCiphers().includes('sm4-gcm'));
assert.strictEqual(crypto.getCipherInfo('sm4-gcm'), undefined);
assert.throws(() => crypto.createCipheriv('sm4-gcm', key, iv), {
  code: 'ERR_CRYPTO_UNKNOWN_CIPHER',
});

// Disabling FIPS again must make it available once more.
crypto.setFips(false);
assert.strictEqual(crypto.getFips(), 0);

assert(crypto.getCiphers().includes('sm4-gcm'));
crypto.createCipheriv('sm4-gcm', key, iv);
