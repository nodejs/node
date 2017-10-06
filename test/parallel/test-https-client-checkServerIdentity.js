'use strict';
const common = require('../common');
if (!common.hasCrypto)
  common.skip('missing crypto');

const assert = require('assert');
const fixtures = require('../common/fixtures');
const https = require('https');

const options = {
  key: fixtures.readKey('agent3-key.pem'),
  cert: fixtures.readKey('agent3-cert.pem')
};

const server = https.createServer(options, common.mustCall(function(req, res) {
  res.writeHead(200);
  res.end();
  req.resume();
})).listen(0, function() {
  authorized();
});

function authorized() {
  const req = https.request({
    port: server.address().port,
    rejectUnauthorized: true,
    ca: [fixtures.readKey('ca2-cert.pem')]
  }, common.mustNotCall());
  req.on('error', function(err) {
    override();
  });
  req.end();
}

function override() {
  const options = {
    port: server.address().port,
    rejectUnauthorized: true,
    ca: [fixtures.readKey('ca2-cert.pem')],
    checkServerIdentity: function(host, cert) {
      return false;
    }
  };
  options.agent = new https.Agent(options);
  const req = https.request(options, function(res) {
    assert(req.socket.authorized);
    server.close();
  });
  req.on('error', function(err) {
    throw err;
  });
  req.end();
}
