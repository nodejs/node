'use strict';

const common = require('../common');
if (!common.hasCrypto)
  common.skip('missing crypto');

const assert = require('assert');
const https = require('https');

const agent = new https.Agent();

// empty options
assert.strictEqual(
  agent.getName({}),
  'localhost:::::::::::::::::::'
);

// pass all options arguments
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
  sessionIdContext: 'sessionIdContext'
};

assert.strictEqual(
  agent.getName(options),
  '0.0.0.0:443:192.168.1.1:ca:cert:dynamic:ciphers:key:pfx:false:localhost:' +
    '::secureProtocol:c,r,l:false:ecdhCurve:dhparam:0:sessionIdContext'
);
