'use strict';
const common = require('../common');
const fixtures = require('../common/fixtures');
if (!common.hasCrypto)
  common.skip('missing crypto');

const assert = require('assert');
const https = require('https');

const options = {
  key: fixtures.readKey('agent1-key.pem'),
  cert: fixtures.readKey('agent1-cert.pem')
};

const TOTAL = 4;
let waiting = TOTAL;

const server = https.Server(options, function(req, res) {
  if (--waiting === 0) server.close();

  const servername = req.socket.servername;

  if (servername !== false) {
    res.setHeader('x-sni', servername);
  }

  res.end('hello world');
});

server.listen(0, function() {
  function expectResponse(id) {
    return common.mustCall(function(res) {
      res.resume();
      assert.strictEqual(res.headers['x-sni'],
                         id === false ? undefined : `sni.${id}`);
    });
  }

  const agent = new https.Agent({
    maxSockets: 1
  });
  for (let j = 0; j < TOTAL; j++) {
    https.get({
      agent: agent,

      path: '/',
      port: this.address().port,
      host: '127.0.0.1',
      servername: `sni.${j}`,
      rejectUnauthorized: false
    }, expectResponse(j));
  }
  https.get({
    agent: agent,

    path: '/',
    port: this.address().port,
    host: '127.0.0.1',
    servername: '',
    rejectUnauthorized: false
  }, expectResponse(false));
});
