'use strict';

const common = require('../common');
if (!common.hasCrypto)
  common.skip('missing crypto');

const assert = require('assert');
const https = require('https');
const fixtures = require('../common/fixtures');

const agent = new https.Agent();

// empty argument
assert.strictEqual(
  agent.getName(),
  'localhost::::::::::::::::::::::'
);

// empty options
assert.strictEqual(
  agent.getName({}),
  'localhost::::::::::::::::::::::'
);

// Pass all options arguments
const options = {
  host: '0.0.0.0',
  port: 443,
  localAddress: '192.168.1.1',
  ca: 'ca',
  cert: 'cert',
  clientCertEngine: 'dynamic',
  ciphers: 'ciphers',
  crl: [Buffer.from('c'), Buffer.from('r'), Buffer.from('l')],
  dhparam: 'dhparam',
  ecdhCurve: 'ecdhCurve',
  honorCipherOrder: false,
  key: 'key',
  pfx: 'pfx',
  rejectUnauthorized: false,
  secureOptions: 0,
  secureProtocol: 'secureProtocol',
  servername: 'localhost',
  sessionIdContext: 'sessionIdContext',
  sigalgs: 'sigalgs',
  privateKeyIdentifier: 'privateKeyIdentifier',
  privateKeyEngine: 'privateKeyEngine',
};

assert.strictEqual(
  agent.getName(options),
  '0.0.0.0:443:192.168.1.1:ca:cert:dynamic:ciphers:key:pfx:false:localhost:' +
    '::secureProtocol:c,r,l:false:ecdhCurve:dhparam:0:sessionIdContext:' +
    '"sigalgs":privateKeyIdentifier:privateKeyEngine'
);

{
  const baseOptions = {
    host: '0.0.0.0',
    port: 443,
  };

  const agent1 = fixtures.readKey('agent1.pfx');
  const agent6 = fixtures.readKey('agent6.pfx');

  assert.notStrictEqual(
    agent.getName({
      ...baseOptions,
      pfx: [{ buf: agent1, passphrase: 'sample' }],
    }),
    agent.getName({
      ...baseOptions,
      pfx: [{ buf: agent6, passphrase: 'sample' }],
    })
  );

  assert.notStrictEqual(
    agent.getName({
      ...baseOptions,
      pfx: [{ buf: agent1, passphrase: 'sample' }],
    }),
    agent.getName({
      ...baseOptions,
      pfx: [{ buf: agent1, passphrase: 'different' }],
    })
  );

  assert.notStrictEqual(
    agent.getName({
      ...baseOptions,
      pfx: [{ __proto__: { buf: agent1, passphrase: 'sample' } }],
    }),
    agent.getName({
      ...baseOptions,
      pfx: [{ __proto__: { buf: agent6, passphrase: 'sample' } }],
    })
  );
}
