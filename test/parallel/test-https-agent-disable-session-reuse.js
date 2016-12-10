'use strict';
const common = require('../common');
const assert = require('assert');

if (!common.hasCrypto) {
  common.skip('missing crypto');
  return;
}

const TOTAL_REQS = 2;

const https = require('https');

const fs = require('fs');

const options = {
  key: fs.readFileSync(common.fixturesDir + '/keys/agent1-key.pem'),
  cert: fs.readFileSync(common.fixturesDir + '/keys/agent1-cert.pem')
};

const clientSessions = [];
var serverRequests = 0;

const agent = new https.Agent({
  maxCachedSessions: 0
});

const server = https.createServer(options, function(req, res) {
  serverRequests++;
  res.end('ok');
}).listen(0, function() {
  var waiting = TOTAL_REQS;
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
  assert.equal(serverRequests, TOTAL_REQS);
  assert.equal(clientSessions.length, TOTAL_REQS);
  assert.notEqual(clientSessions[0].toString('hex'),
                  clientSessions[1].toString('hex'));
});
