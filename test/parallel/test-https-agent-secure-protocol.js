'use strict';
const common = require('../common');
if (!common.hasCrypto)
  common.skip('missing crypto');

const assert = require('assert');
const https = require('https');
const fs = require('fs');

const options = {
  key: fs.readFileSync(`${common.fixturesDir}/keys/agent1-key.pem`),
  cert: fs.readFileSync(`${common.fixturesDir}/keys/agent1-cert.pem`),
  ca: fs.readFileSync(`${common.fixturesDir}/keys/ca1-cert.pem`)
};

const server = https.Server(options, function(req, res) {
  res.writeHead(200);
  res.end('hello world\n');
});

server.listen(0, common.mustCall(function() {
  const port = this.address().port;
  const globalAgent = https.globalAgent;
  globalAgent.keepAlive = true;
  https.get({
    path: '/',
    port: port,
    ca: options.ca,
    rejectUnauthorized: true,
    servername: 'agent1',
    secureProtocol: 'SSLv23_method'
  }, common.mustCall(function(res) {
    res.resume();
    globalAgent.once('free', common.mustCall(function() {
      https.get({
        path: '/',
        port: port,
        ca: options.ca,
        rejectUnauthorized: true,
        servername: 'agent1',
        secureProtocol: 'TLSv1_method'
      }, common.mustCall(function(res) {
        res.resume();
        globalAgent.once('free', common.mustCall(function() {
          // Verify that two keep-alived connections are created
          // due to the different secureProtocol settings:
          const keys = Object.keys(globalAgent.freeSockets);
          assert.strictEqual(keys.length, 2);
          assert.ok(keys[0].includes(':SSLv23_method'));
          assert.ok(keys[1].includes(':TLSv1_method'));
          globalAgent.destroy();
          server.close();
        }));
      }));
    }));
  }));
}));
