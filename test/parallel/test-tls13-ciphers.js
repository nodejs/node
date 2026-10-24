'use strict';
const common = require('../common');
if (!common.hasCrypto)
  common.skip('missing crypto');

const assert = require('assert');
const { constants } = require('crypto');
const tls = require('tls');
const fixtures = require('../common/fixtures');
const { hasOpenSSL, isBoringSSL } = require('../common/crypto');
const { spawnSyncAndAssert } = require('../common/child_process');
const { connect, keys } = require(fixtures.path('tls-connect'));

const aes128 = 'TLS_AES_128_GCM_SHA256';
const aes256 = 'TLS_AES_256_GCM_SHA384';
const tls13Ciphers = [aes128, aes256, 'TLS_CHACHA20_POLY1305_SHA256'];
const legacy = 'ECDHE-RSA-AES256-GCM-SHA384';

function test(clientCiphers, serverCiphers, expectedCipher, options = {}) {
  connect({
    client: {
      rejectUnauthorized: false,
      minVersion: 'TLSv1.2',
      maxVersion: 'TLSv1.3',
      ciphers: clientCiphers,
      ...options.client,
    },
    server: {
      key: keys.agent2.key,
      cert: keys.agent2.cert,
      minVersion: 'TLSv1.2',
      maxVersion: 'TLSv1.3',
      ciphers: serverCiphers,
      ...options.server,
    },
  }, common.mustCall((err, pair, cleanup) => {
    if (expectedCipher === undefined) {
      assert(err);
      assert.strictEqual(pair.server.err.code, 'ERR_SSL_NO_SHARED_CIPHER');
      assert(pair.client.err);
    } else {
      assert.ifError(err);
      const expected = Array.isArray(expectedCipher) ? expectedCipher : [expectedCipher];
      const cipher = pair.client.conn.getCipher().name;
      assert(expected.includes(cipher));
      for (const { conn } of [pair.client, pair.server]) {
        assert.strictEqual(conn.getCipher().name, cipher);
        assert.strictEqual(conn.getProtocol(),
                           expectedCipher === legacy ? 'TLSv1.2' : 'TLSv1.3');
      }
    }
    cleanup();
  }));
}

function testLegacyOnlyCipherLists() {
  for (const honorCipherOrder of [true, false]) {
    const options = { server: { honorCipherOrder } };
    // A legacy-only list retains the backend's TLS 1.3 defaults. Check that
    // each peer can negotiate either AES suite without assuming their order.
    test(aes128, legacy, aes128, options);
    test(aes256, legacy, aes256, options);
    test(legacy, aes128, aes128, options);
    test(legacy, aes256, aes256, options);
    test(legacy, legacy, tls13Ciphers, options);
  }
}

if (process.argv[2] !== undefined) {
  const mode = process.argv[2];
  if (mode === 'runtime-defaults') {
    tls.DEFAULT_CIPHERS = aes128;
  } else {
    assert.strictEqual(tls.DEFAULT_CIPHERS,
                       mode === 'cli-defaults' ? aes128 : legacy);
  }
  // Explicit TLS 1.3 defaults are still applied when ciphers is omitted, but
  // must not replace the native defaults for an explicit legacy-only list.
  test(undefined, undefined, mode === 'cli-legacy-defaults' ? tls13Ciphers : aes128);
  testLegacyOnlyCipherLists();
  return;
}

// Both peers must apply the configured TLS 1.3 cipher list.
test(aes256, undefined, aes256);
test(undefined, aes256, aes256);
test(aes128, aes256, undefined);

// Duplicate names and trailing whitespace must not cause rejection.
const duplicates = `${aes256} \t:${aes256}`;
test(duplicates, duplicates, aes256);

// OpenSSL 1.1.1 rejects unknown TLS 1.3 cipher names. OpenSSL 3 and BoringSSL
// ignore them and do not interpret exclusion rules in the TLS 1.3 list.
const list = `TLS_UNKNOWN:!${aes128}:${duplicates}`;
if (hasOpenSSL(3) || isBoringSSL) {
  test(list, list, aes256);
} else {
  assert.throws(() => tls.createSecureContext({ ciphers: list }), {
    code: 'ERR_SSL_NO_CIPHER_MATCH',
  });
}
for (const ciphers of ['TLS_UNKNOWN', `!${aes128}`]) {
  assert.throws(() => tls.createSecureContext({ ciphers }), {
    code: 'ERR_SSL_NO_CIPHER_MATCH',
  });
}
if (isBoringSSL) {
  assert.throws(() => tls.createSecureContext({
    ciphers: 'TLS_AES_128_CCM_SHA256',
  }), {
    code: 'ERR_SSL_NO_CIPHER_MATCH',
  });
}

// Honor the server's cipher-order option in TLS 1.3 as well as TLS 1.2.
test(`${aes256}:${aes128}`, `${aes128}:${aes256}`, aes128, {
  server: { honorCipherOrder: true },
});
test(`${aes256}:${aes128}`, `${aes128}:${aes256}`, aes256, {
  server: { honorCipherOrder: false },
});
testLegacyOnlyCipherLists();

// TLS 1.2 configuration and explicit protocol restrictions remain effective.
const mixed = `${legacy}:${aes256}`;
test(mixed, mixed, aes256);
test(mixed, mixed, legacy, {
  client: { maxVersion: 'TLSv1.2' },
  server: { maxVersion: 'TLSv1.2' },
});
test(mixed, mixed, legacy, {
  server: { secureOptions: constants.SSL_OP_NO_TLSv1_3 },
});

// Empty public cipher strings continue to select Node's defaults.
test('', aes256, aes256);
test(aes256, '', aes256);

for (const [mode, ciphers] of [
  ['runtime-defaults', undefined],
  ['cli-defaults', aes128],
  ['cli-legacy-defaults', legacy],
]) {
  spawnSyncAndAssert(process.execPath, [
    ...process.execArgv,
    ...(ciphers === undefined ? [] : [`--tls-cipher-list=${ciphers}`]),
    __filename,
    mode,
  ], {});
}
