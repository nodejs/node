'use strict';
const common = require('../common');
if (!common.hasCrypto)
  common.skip('missing crypto');

if (process.features.openssl_is_boringssl) {
  require('../common/boringssl').testEphemeralKeyInfo();
  return;
}

const fixtures = require('../common/fixtures');
const { hasOpenSSL, hasFIPS } = require('../common/crypto');

const assert = require('assert');
const { X509Certificate } = require('crypto');
const tls = require('tls');

const key = fixtures.readKey('agent2-key.pem');
const cert = fixtures.readKey('agent2-cert.pem');
const fips3 = hasFIPS(3);
const rejectsXCurves = hasFIPS(3, 5);

function loadDHParam(n) {
  return fixtures.readKey(`dh${n}.pem`);
}

function test(size, type, name, cipher, expectError = false) {
  assert(cipher);

  const options = {
    key: key,
    cert: cert,
    ciphers: cipher,
    maxVersion: 'TLSv1.2',
  };

  if (name) options.ecdhCurve = name;

  if (type === 'DH') {
    if (size === 'auto') {
      options.dhparam = 'auto';
      // The DHE parameters selected by OpenSSL depend on the strength of the
      // certificate's key. For this test, we can assume that the modulus length
      // of the certificate's key is equal to the size of the DHE parameter, but
      // that is really only true for a few modulus lengths.
      ({
        publicKey: { asymmetricKeyDetails: { modulusLength: size } }
      } = new X509Certificate(cert));
    } else {
      options.dhparam = loadDHParam(size);
    }
  }

  if (rejectsXCurves && (name === 'X25519' || name === 'X448')) {
    assert.throws(() => tls.createServer(options), {
      code: 'ERR_CRYPTO_OPERATION_FAILED',
    });
    return;
  }

  const onConnection = expectError ? common.mustNotCall() :
    common.mustCall((conn) => {
      assert.strictEqual(conn.getEphemeralKeyInfo(), null);
      conn.end();
    });
  const server = tls.createServer(options, onConnection);

  server.on('close', common.mustSucceed());

  server.listen(0, common.mustCall(() => {
    const onSecureConnect = expectError ? common.mustNotCall() :
      common.mustCall(function() {
        const ekeyinfo = client.getEphemeralKeyInfo();
        assert.strictEqual(ekeyinfo.type, type);
        assert.strictEqual(ekeyinfo.size, size);
        assert.strictEqual(ekeyinfo.name, name);
        server.close();
      });
    const client = tls.connect({
      port: server.address().port,
      rejectUnauthorized: false
    }, onSecureConnect);
    if (expectError) {
      client.on('error', common.mustCall((err) => {
        assert.strictEqual(err.code, 'ERR_SSL_BAD_DH_VALUE');
        server.close();
      }));
    } else {
      client.on('secureConnect', common.mustCall());
    }
  }));
}

if (!fips3)
  test(undefined, undefined, undefined, 'AES256-SHA256');
test('auto', 'DH', undefined, 'DHE-RSA-AES256-GCM-SHA384');
if (fips3 && !hasOpenSSL(4)) {
  test(2048, 'DH', undefined, 'DHE-RSA-AES256-GCM-SHA384', true);
} else {
  if (hasOpenSSL(4, 0)) {
    // OpenSSL 4.0 implements RFC 7919 FFDHE negotiation for TLS 1.2 and
    // always selects FFDHE-2048 regardless of the server-supplied dhparam.
  } else if (!hasOpenSSL(3, 2)) {
    test(1024, 'DH', undefined, 'DHE-RSA-AES256-GCM-SHA384');
  } else {
    test(3072, 'DH', undefined, 'DHE-RSA-AES256-GCM-SHA384');
  }
  test(2048, 'DH', undefined, 'DHE-RSA-AES256-GCM-SHA384');
}
test(256, 'ECDH', 'prime256v1', 'ECDHE-RSA-AES256-GCM-SHA384');
test(521, 'ECDH', 'secp521r1', 'ECDHE-RSA-AES256-GCM-SHA384');
test(253, 'ECDH', 'X25519', 'ECDHE-RSA-AES256-GCM-SHA384');
test(448, 'ECDH', 'X448', 'ECDHE-RSA-AES256-GCM-SHA384');

function testTLS13Group(size, type, name) {
  const options = {
    key,
    cert,
    ecdhCurve: name,
    minVersion: 'TLSv1.3',
    maxVersion: 'TLSv1.3',
  };

  const unsupportedFipsGroup =
    (rejectsXCurves && name === 'X25519') ||
    (hasFIPS(4) &&
     (name === 'curveSM2' || name === 'curveSM2MLKEM768'));
  if (unsupportedFipsGroup) {
    assert.throws(() => tls.createServer(options), {
      code: 'ERR_CRYPTO_OPERATION_FAILED',
    });
    return;
  }

  const server = tls.createServer(options, common.mustCall((conn) => {
    assert.strictEqual(conn.getEphemeralKeyInfo(), null);
    conn.end();
  }));

  server.on('close', common.mustSucceed());

  server.listen(0, common.mustCall(() => {
    const client = tls.connect({
      port: server.address().port,
      rejectUnauthorized: false,
      ecdhCurve: name,
      minVersion: 'TLSv1.3',
      maxVersion: 'TLSv1.3',
    }, common.mustCall(() => {
      const ekeyinfo = client.getEphemeralKeyInfo();
      assert.strictEqual(ekeyinfo.type, type);
      assert.strictEqual(ekeyinfo.size, size);
      assert.strictEqual(ekeyinfo.name, name);
      server.close();
    }));
    client.on('secureConnect', common.mustCall());
  }));
}

if (fips3)
  testTLS13Group(256, 'ECDH', 'prime256v1');
testTLS13Group(253, 'ECDH', 'X25519');

if (hasOpenSSL(3, 5)) {
  const tls13Groups = [
    'MLKEM512',
    'MLKEM768',
    'MLKEM1024',
    'SecP256r1MLKEM768',
    'X25519MLKEM768',
    'SecP384r1MLKEM1024',
  ];

  if (hasOpenSSL(4, 0)) {
    tls13Groups.push('curveSM2');
    tls13Groups.push('curveSM2MLKEM768');
  }

  tls13Groups.forEach((name) => testTLS13Group(undefined, 'TLSGroup', name));
}
