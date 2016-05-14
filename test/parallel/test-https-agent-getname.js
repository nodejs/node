'use strict';

require('../common');
var assert = require('assert');
var https = require('https');

var agent = new https.Agent();

// empty options
assert.strictEqual(
  agent.getName({}),
  'localhost:::::::::'
);

// pass all options arguments
var options = {
  host: '0.0.0.0',
  port: 80,
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
  '0.0.0.0:80:192.168.1.1:ca:cert:ciphers:key:pfx:false:localhost'
);
