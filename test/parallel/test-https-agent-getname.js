'use strict';

const common = require('../common');
if (!common.hasCrypto) {
  common.skip('missing crypto');
  return;
}

const assert = require('assert');
const https = require('https');

const agent = new https.Agent();

// empty options
assert.strictEqual(
  agent.getName({}),
  'localhost:::::::::'
);

// pass all options arguments
const options = {
  host: '0.0.0.0',
  port: 443,
  localAddress: '192.168.1.1',
  ca: 'ca',
  cert: 'cert',
  ciphers: 'ciphers',
  key: 'key',
  pfx: 'pfx',
  rejectUnauthorized: false,
  servername: 'localhost',
};

assert.strictEqual(
  agent.getName(options),
  '0.0.0.0:443:192.168.1.1:ca:cert:ciphers:key:pfx:false:localhost'
);
