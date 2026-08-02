'use strict';

const common = require('../common');

if (!common.hasCrypto)
  common.skip('missing crypto');

const assert = require('node:assert');
const tls = require('node:tls');
const { fork } = require('node:child_process');
const { hasFIPS } = require('../common/crypto');
const fixtures = require('../common/fixtures');
const fips3 = hasFIPS(3);

const tests = [
  {
    get clientOptions() {
      const secureContext = tls.createSecureContext();
      secureContext.context.addCACert(
        fixtures.readKey('ca1-cert.pem')
      );

      return {
        secureContext
      };
    }
  },
  {
    clientOptions: {
      crl: fixtures.readKey('ca2-crl.pem')
    }
  },
];

if (fips3) {
  assert.throws(() => tls.createSecureContext({
    pfx: fixtures.readKey('agent1.pfx'),
    passphrase: 'sample',
  }), {
    code: 'ERR_CRYPTO_UNSUPPORTED_OPERATION',
  });
}

if (!fips3 || hasFIPS(3, 5)) {
  tests.push({
    clientOptions: {
      pfx: fixtures.readKey(fips3 ?
        'agent1-fips.pfx' : 'agent1.pfx'),
      passphrase: fips3 ? 'password' : 'sample'
    }
  });
}

if (process.argv[2]) {
  const testNumber = parseInt(process.argv[2], 10);
  assert(testNumber >= 0 && testNumber < tests.length);

  const test = tests[testNumber];

  const clientOptions = {
    ...test.clientOptions,
    port: process.argv[3],
    checkServerIdentity: common.mustCall()
  };

  const client = tls.connect(clientOptions, common.mustCall(() => {
    client.end('hi');
  }));
} else {
  const serverOptions = {
    key: fixtures.readKey('agent3-key.pem'),
    cert: fixtures.readKey('agent3-cert.pem')
  };

  for (const testNumber in tests) {
    const server = tls.createServer(serverOptions, common.mustCall((socket) => {
      socket.end('bye');
      server.close();
    }));

    server.listen(0, common.mustCall(() => {
      const env = {
        ...process.env,
        NODE_EXTRA_CA_CERTS: fixtures.path('keys', 'ca2-cert.pem')
      };

      const args = [
        testNumber,
        server.address().port,
      ];

      fork(__filename, args, { env }).on('exit', common.mustCall((status) => {
        assert.strictEqual(status, 0);
      }));
    }));
  }
}
