'use strict';
const common = require('../common');

// This test ensures that ecdhCurve option of TLS server supports colon
// separated ECDH curve names as value.

if (!common.hasCrypto) {
  common.skip('missing crypto');
}

const {
  opensslCli,
  hasOpenSSL,
  hasFIPS,
} = require('../common/crypto');
const crypto = require('crypto');

if (!opensslCli) {
  common.skip('missing openssl-cli');
}

const assert = require('assert');
const tls = require('tls');
const { execFile } = require('child_process');
const fixtures = require('../common/fixtures');
const fips3 = hasFIPS(3);

function loadPEM(n) {
  return fixtures.readKey(`${n}.pem`);
}

// The FIPS provider and OpenSSL 4.0 disable support for deprecated elliptic
// curves from RFC 8422 (including secp256k1) by default.
const ecdhCurve = process.features.openssl_is_boringssl ||
  hasOpenSSL(4, 0) || hasFIPS(3) ?
  'prime256v1:secp521r1' :
  'secp256k1:prime256v1:secp521r1';

const options = {
  key: loadPEM('agent2-key'),
  cert: loadPEM('agent2-cert'),
  ciphers: '-ALL:ECDHE-RSA-AES128-SHA256',
  ecdhCurve,
  maxVersion: 'TLSv1.2',
};

const reply = 'I AM THE WALRUS'; // Something recognizable

const server = tls.createServer(options, (conn) => {
  conn.end(reply);
}).listen(0, common.mustCall(() => {
  const args = ['s_client',
                '-cipher', `${options.ciphers}`,
                '-connect', `127.0.0.1:${server.address().port}`];

  execFile(opensslCli, args, common.mustSucceed((stdout) => {
    assert(stdout.includes(reply));
    server.close();
  }));
}));

{
  // Some unsupported curves.
  const unsupportedCurves = [
    'wap-wsg-idm-ecid-wtls1',
    'c2pnb163v1',
    'prime192v3',
  ];

  // Setting a Brainpool group on a TLS context is deferred by OpenSSL, so
  // exercise the prohibited key operation directly under FIPS properties.
  if (fips3) {
    if (hasFIPS(3, 5)) {
      assert.throws(
        () => crypto.createECDH('brainpoolP256r1').generateKeys(),
        { code: 'ERR_CRYPTO_OPERATION_FAILED' });
    } else {
      unsupportedCurves.push('brainpoolP256r1');
    }
  } else if (crypto.getFips() === 1) {
    unsupportedCurves.push('brainpoolP256r1');
  }

  // Deprecated RFC 8422 curves are disabled by default in OpenSSL 4.0.
  if (process.features.openssl_is_boringssl || hasOpenSSL(4, 0)) {
    unsupportedCurves.push('secp256k1');
  }

  unsupportedCurves.forEach((ecdhCurve) => {
    assert.throws(() => tls.createServer({ ecdhCurve }),
                  /Error: Failed to set ECDH curve/);
  });
}
