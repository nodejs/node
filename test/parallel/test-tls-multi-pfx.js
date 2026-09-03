'use strict';
const common = require('../common');
if (!common.hasCrypto)
  common.skip('missing crypto');

if (process.features.openssl_is_boringssl) {
  require('../common/boringssl').testMultiPfxSelectionDifference();
  return;
}

const assert = require('assert');
const tls = require('tls');
const { hasFIPS } = require('../common/crypto');
const fixtures = require('../common/fixtures');
const fips3 = hasFIPS(3);
const fips4 = hasFIPS(4);

const legacyOptions = {
  pfx: [
    {
      buf: fixtures.readKey('agent1.pfx'),
      passphrase: 'sample'
    },
    fixtures.readKey('ec.pfx'),
  ]
};

if (fips3) {
  assert.throws(() => tls.createServer(legacyOptions), {
    code: 'ERR_CRYPTO_UNSUPPORTED_OPERATION',
  });

  if (!hasFIPS(3, 5)) {
    return;
  }
}

const fipsPfx = [
  {
    buf: fixtures.readKey('agent1-fips.pfx'),
    passphrase: 'password',
  },
  {
    buf: fixtures.readKey('ec-fips.pfx'),
    passphrase: 'password',
  },
];

if (fips4) {
  for (const { buf } of fipsPfx) {
    assert.throws(() => tls.createServer({
      pfx: buf,
      passphrase: 'sample',
    }), {
      message: 'password strength too weak',
    });
  }
}

const options = fips3 ? { pfx: fipsPfx } : legacyOptions;

const ciphers = [];

const server = tls.createServer(options, function(conn) {
  conn.end('ok');
}).listen(0, common.mustCall(function() {
  const ecdsa = tls.connect(this.address().port, {
    ciphers: 'ECDHE-ECDSA-AES256-GCM-SHA384',
    maxVersion: 'TLSv1.2',
    rejectUnauthorized: false,
  }, common.mustCall(function() {
    ciphers.push(ecdsa.getCipher());
    const rsa = tls.connect(server.address().port, {
      ciphers: 'ECDHE-RSA-AES256-GCM-SHA384',
      maxVersion: 'TLSv1.2',
      rejectUnauthorized: false,
    }, common.mustCall(function() {
      ciphers.push(rsa.getCipher());
      ecdsa.end();
      rsa.end();
      server.close();
    }));
  }));
}));

process.on('exit', function() {
  assert.deepStrictEqual(ciphers, [{
    name: 'ECDHE-ECDSA-AES256-GCM-SHA384',
    standardName: 'TLS_ECDHE_ECDSA_WITH_AES_256_GCM_SHA384',
    version: 'TLSv1.2'
  }, {
    name: 'ECDHE-RSA-AES256-GCM-SHA384',
    standardName: 'TLS_ECDHE_RSA_WITH_AES_256_GCM_SHA384',
    version: 'TLSv1.2'
  }]);
});
