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
  'localhost::::::::::::::::::::::::::::'
);

// empty options
assert.strictEqual(
  agent.getName({}),
  'localhost::::::::::::::::::::::::::::'
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
  '0.0.0.0:443:192.168.1.1:s2:ca:s4:cert:s7:ciphers:s3:key::' +
    's3:pfx::b5:false:s9:localhost:::s14:secureProtocol:' +
    'a3:7:v1:Yw==:7:v1:cg==:7:v1:bA==:b5:false:s9:ecdhCurve:s7:dhparam:' +
    'n1:0:s16:sessionIdContext:s7:sigalgs:::::::::'
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

  assert.notStrictEqual(
    agent.getName(baseOptions),
    agent.getName({ ...baseOptions, allowRenegotiation: false })
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

  // Values containing the field delimiter must not shift across fields.
  const collisions = [
    [{ servername: 'a:b' }, { servername: 'a', minVersion: 'b:' }],
    [{ minVersion: 'a:b' }, { minVersion: 'a', maxVersion: 'b:' }],
    [{ maxVersion: 'a:b' }, { maxVersion: 'a', secureProtocol: 'b:' }],
    [{ rejectUnauthorized: 'false:a' },
     { rejectUnauthorized: 'false', servername: 'a:' }],
    [{ honorCipherOrder: 'a:b' }, { honorCipherOrder: 'a', ecdhCurve: 'b:' }],
    [{ ecdhCurve: 'a:b' }, { ecdhCurve: 'a', dhparam: 'b:' }],
    [{ dhparam: 'a:b' }, { dhparam: 'a', secureOptions: 'b:' }],
    [{ secureOptions: '1:a' }, { secureOptions: 1, sessionIdContext: 'a:' }],
    [{ sessionIdContext: 'a:b' }, { sessionIdContext: 'a', sigalgs: 'b:' }],
    [{ sigalgs: ['a:b'] }, { sigalgs: ['a', 'b'] }],
    [{ servername: 'a', rejectUnauthorized: false, ecdhCurve: 'P-256:P-384' },
     { rejectUnauthorized: 'false:a', honorCipherOrder: 'P-256',
       ecdhCurve: 'P-384' }],
  ];
  for (const [first, second] of collisions) {
    assert.notStrictEqual(
      agent.getName({ ...baseOptions, ...first }),
      agent.getName({ ...baseOptions, ...second }),
      `${JSON.stringify(first)} collides with ${JSON.stringify(second)}`
    );
  }

  // The type of a value is part of its key.
  assert.notStrictEqual(
    agent.getName({ ...baseOptions, rejectUnauthorized: false }),
    agent.getName({ ...baseOptions, rejectUnauthorized: 'false' })
  );

  assert.notStrictEqual(
    agent.getName({ ...baseOptions, dhparam: new DataView(new ArrayBuffer(2)) }),
    agent.getName({
      ...baseOptions,
      dhparam: new DataView(new Uint8Array([1, 2]).buffer),
    })
  );
}
