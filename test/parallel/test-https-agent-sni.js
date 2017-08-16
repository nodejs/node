'use strict';
const common = require('../common');
if (!common.hasCrypto)
  common.skip('missing crypto');

const assert = require('assert');
const https = require('https');
const fs = require('fs');

const options = {
  key: fs.readFileSync(`${common.fixturesDir}/keys/agent1-key.pem`),
  cert: fs.readFileSync(`${common.fixturesDir}/keys/agent1-cert.pem`)
};

const TOTAL = 4;
let waiting = TOTAL;

const server = https.Server(options, function(req, res) {
  if (--waiting === 0) server.close();

  res.writeHead(200, {
    'x-sni': req.socket.servername
  });
  res.end('hello world');
});

server.listen(0, function() {
  function expectResponse(id) {
    return common.mustCall(function(res) {
      res.resume();
      assert.strictEqual(res.headers['x-sni'], `sni.${id}`);
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
});
