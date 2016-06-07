'use strict';
const common = require('../common');
const assert = require('assert');
const https = require('https');
const fs = require('fs');

const options1 = {
  key: fs.readFileSync(common.fixturesDir + '/keys/agent1-key.pem', 'ascii'),
  crt: fs.readFileSync(common.fixturesDir + '/keys/agent1-cert.pem', 'ascii')
};

const options2 = {
  ky: fs.readFileSync(common.fixturesDir + '/keys/agent1-key.pem', 'ascii'),
  cert: fs.readFileSync(common.fixturesDir + '/keys/agent1-cert.pem', 'ascii')
};

assert.throws(() => https.createServer(options1, assert.fail),
'cert is a required parameter for Server.createServer');

assert.throws(() => https.createServer(options2, assert.fail),
'key is a required parameter for Server.createServer');
