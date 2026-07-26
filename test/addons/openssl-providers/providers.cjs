'use strict';

const common = require('../../common');
if (!common.hasCrypto) {
  common.skip('missing crypto');
}
const { isBoringSSL } = require('../../common/crypto');

if (isBoringSSL) {
  common.skip('OpenSSL provider support is required');
}
const assert = require('node:assert');
const {
  createCipheriv,
  createDecipheriv,
  createHash,
  getCiphers,
  getHashes,
} = require('node:crypto');
const { debuglog } = require('node:util');
const { getProviders } = require(`./build/${common.buildType}/binding`);

// For the providers defined here, the expectation is that the listed ciphers
// and hash algorithms are only provided by the named provider. These are for
// basic checks and are not intended to list every cipher or hash algorithm
// supported by the provider.
const providers = {
  'default': {
    ciphers: [
      {
        name: 'aes-128-cbc-cts',
        keyLength: 16,
        ivLength: 16,
        plaintextLength: 32,
        unavailableCode: 'ERR_CRYPTO_UNKNOWN_CIPHER',
      },
      {
        name: 'des3-wrap',
        keyLength: 24,
        ivLength: null,
        plaintextLength: 16,
        unavailableCode: 'ERR_OSSL_EVP_UNSUPPORTED',
      },
    ],
    hashes: [
      'sha512-256',
      ...['keccak-kmac-128', 'keccak-kmac128']
        .filter((name) => getHashes().includes(name)),
    ],
  },
  'legacy': {
    ciphers: [
      {
        name: 'blowfish',
        keyLength: 16,
        ivLength: 8,
        plaintextLength: 16,
        unavailableCode: 'ERR_OSSL_EVP_UNSUPPORTED',
      },
      {
        name: 'idea',
        keyLength: 16,
        ivLength: 8,
        plaintextLength: 16,
        unavailableCode: 'ERR_OSSL_EVP_UNSUPPORTED',
      },
    ],
    hashes: ['md4', 'whirlpool'],
  },
};

const debug = debuglog('test');

module.exports = {
  getCurrentProviders: getProviders,
  testProviderPresent,
  testProviderAbsent,
};

function assertArrayDoesNotInclude(array, item, desc) {
  assert(!array.includes(item),
         `${desc} [${array}] unexpectedly includes "${item}"`);
}

function assertArrayIncludes(array, item, desc) {
  assert(array.includes(item),
         `${desc} [${array}] does not include "${item}"`);
}

function createSupportedHash(hash) {
  try {
    return createHash(hash);
  } catch (err) {
    if (err?.code !== 'ERR_OSSL_EVP_NOT_XOF_OR_INVALID_LENGTH') throw err;
    return createHash(hash, { outputLength: 32 });
  }
}

function assertCipherRoundTrip({
  name,
  keyLength,
  ivLength,
  plaintextLength,
}) {
  const key = Buffer.alloc(keyLength, 0x42);
  const iv = ivLength === null ? null : Buffer.alloc(ivLength, 0x24);
  const plaintext = Buffer.alloc(plaintextLength, 0x61);

  const cipher = createCipheriv(name, key, iv);
  const ciphertext = Buffer.concat([
    cipher.update(plaintext),
    cipher.final(),
  ]);
  const decipher = createDecipheriv(name, key, iv);
  const received = Buffer.concat([
    decipher.update(ciphertext),
    decipher.final(),
  ]);
  assert.deepStrictEqual(received, plaintext);
}

function testProviderPresent(provider) {
  debug(`Checking '${provider}' is present`);
  assertArrayIncludes(getProviders(), provider, 'Loaded providers');
  for (const cipher of providers[provider].ciphers || []) {
    debug(`Checking '${cipher.name}' cipher is available`);
    assertArrayIncludes(getCiphers(), cipher.name, 'Available ciphers');
    assertCipherRoundTrip(cipher);
  }
  for (const hash of providers[provider].hashes || []) {
    debug(`Checking '${hash}' hash is available`);
    assertArrayIncludes(getHashes(), hash, 'Available hashes');
    createSupportedHash(hash);
  }
}

function testProviderAbsent(provider) {
  debug(`Checking '${provider}' is absent`);
  assertArrayDoesNotInclude(getProviders(), provider, 'Loaded providers');
  for (const cipher of providers[provider].ciphers || []) {
    const { name, keyLength, ivLength, unavailableCode } = cipher;
    debug(`Checking '${name}' cipher is unavailable`);
    assertArrayDoesNotInclude(getCiphers(), name, 'Available ciphers');
    const key = Buffer.alloc(keyLength, 0x42);
    const iv = ivLength === null ? null : Buffer.alloc(ivLength, 0x24);
    assert.throws(() => createCipheriv(name, key, iv), {
      code: unavailableCode,
    });
  }
  for (const hash of providers[provider].hashes || []) {
    debug(`Checking '${hash}' hash is unavailable`);
    assertArrayDoesNotInclude(getHashes(), hash, 'Available hashes');
    assert.throws(() => { createHash(hash); }, { code: 'ERR_OSSL_EVP_UNSUPPORTED' });
  }
}
