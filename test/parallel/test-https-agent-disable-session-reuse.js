'use strict';
const common = require('../common');
if (!common.hasCrypto)
  common.skip('missing crypto');

const assert = require('assert');
const https = require('https');
const fs = require('fs');

const TOTAL_REQS = 2;

const options = {
  key: fs.readFileSync(`${common.fixturesDir}/keys/agent1-key.pem`),
  cert: fs.readFileSync(`${common.fixturesDir}/keys/agent1-cert.pem`)
};

const clientSessions = [];
let serverRequests = 0;

const agent = new https.Agent({
  maxCachedSessions: 0
});

const server = https.createServer(options, function(req, res) {
  serverRequests++;
  res.end('ok');
}).listen(0, function() {
  let waiting = TOTAL_REQS;
  function request() {
    const options = {
      agent: agent,
      port: server.address().port,
      rejectUnauthorized: false
    };

    https.request(options, function(res) {
      clientSessions.push(res.socket.getSession());

      res.resume();
      res.on('end', function() {
        if (--waiting !== 0)
          return request();
        server.close();
      });
    }).end();
  }
  request();
});

process.on('exit', function() {
  assert.strictEqual(serverRequests, TOTAL_REQS);
  assert.strictEqual(clientSessions.length, TOTAL_REQS);
  assert.notStrictEqual(clientSessions[0].toString('hex'),
                        clientSessions[1].toString('hex'));
});
