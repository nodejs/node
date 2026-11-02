'use strict';

const common = require('../common');
if (!common.hasCrypto)
  common.skip('missing crypto');

const assert = require('assert');
const https = require('https');
const tls = require('tls');
const fixtures = require('../common/fixtures');

const agent = new https.Agent();

// empty argument
assert.strictEqual(
  agent.getName(),
  'localhost:::::::::::::::::::::::::::'
);

// empty options
assert.strictEqual(
  agent.getName({}),
  'localhost:::::::::::::::::::::::::::'
);

// Pass all options arguments
const options = {
  host: '0.0.0.0',
  port: 443,
  localAddress: '192.168.1.1',
  ca: 'ca',
  cert: 'cert',
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
};

assert.strictEqual(
  agent.getName(options),
  '0.0.0.0:443:192.168.1.1:s2:ca:s4:cert:ciphers:s3:key::' +
    's3:pfx::false:localhost:::secureProtocol:' +
    'a3:7:v1:Yw==:7:v1:cg==:7:v1:bA==:false:ecdhCurve:dhparam:0:' +
    'sessionIdContext:"sigalgs"::::::::'
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

  assert.notStrictEqual(
    agent.getName(baseOptions),
    agent.getName({ ...baseOptions, ca: [] })
  );

  assert.notStrictEqual(
    agent.getName({ ...baseOptions, allowPartialTrustChain: true }),
    agent.getName({ ...baseOptions, allowPartialTrustChain: false })
  );

  const secureContext1 = tls.createSecureContext({ ca: agent1 });
  const secureContext2 = tls.createSecureContext({ ca: agent6 });
  assert.strictEqual(
    agent.getName({ ...baseOptions, secureContext: secureContext1 }),
    agent.getName({ ...baseOptions, secureContext: secureContext1 })
  );
  assert.notStrictEqual(
    agent.getName({ ...baseOptions, secureContext: secureContext1 }),
    agent.getName({ ...baseOptions, secureContext: secureContext2 })
  );

  assert.notStrictEqual(
    agent.getName({
      ...baseOptions,
      key: [{ pem: agent1, passphrase: 'sample' }],
    }),
    agent.getName({
      ...baseOptions,
      key: [{ pem: agent6, passphrase: 'sample' }],
    })
  );

  assert.notStrictEqual(
    agent.getName({ ...baseOptions, ciphers: 'A:B' }),
    agent.getName({ ...baseOptions, ciphers: 'A', key: 'B' })
  );

  assert.notStrictEqual(
    agent.getName({ ...baseOptions, ALPNProtocols: ['h2'] }),
    agent.getName({ ...baseOptions, ALPNProtocols: ['http/1.1'] })
  );

  assert.notStrictEqual(
    agent.getName({ ...baseOptions, minDHSize: 1024 }),
    agent.getName({ ...baseOptions, minDHSize: 2048 })
  );

  assert.notStrictEqual(
    agent.getName({ ...baseOptions, pskCallback() {} }),
    agent.getName({ ...baseOptions, pskCallback() {} })
  );

  assert.notStrictEqual(
    agent.getName({ ...baseOptions, session: Buffer.from('one') }),
    agent.getName({ ...baseOptions, session: Buffer.from('two') })
  );
}
