'use strict';
var common = require('../common');
var assert = require('assert');

if (!common.hasCrypto) {
  console.log('1..0 # Skipped: missing crypto');
  return;
}
var https = require('https');

var fs = require('fs');
var path = require('path');

var options = {
  key: fs.readFileSync(path.join(common.fixturesDir, 'keys/agent3-key.pem')),
  cert: fs.readFileSync(path.join(common.fixturesDir, 'keys/agent3-cert.pem'))
};

var reqCount = 0;

var server = https.createServer(options, function(req, res) {
  ++reqCount;
  res.writeHead(200);
  res.end();
  req.resume();
}).listen(common.PORT, function() {
  authorized();
});

function authorized() {
  var req = https.request({
    port: common.PORT,
    rejectUnauthorized: true,
    ca: [fs.readFileSync(path.join(common.fixturesDir, 'keys/ca2-cert.pem'))]
  }, function(res) {
    assert(false);
  });
  req.on('error', function(err) {
    override();
  });
  req.end();
}

function override() {
  var options = {
    port: common.PORT,
    rejectUnauthorized: true,
    ca: [fs.readFileSync(path.join(common.fixturesDir, 'keys/ca2-cert.pem'))],
    checkServerIdentity: function(host, cert) {
      return false;
    }
  };
  options.agent = new https.Agent(options);
  var req = https.request(options, function(res) {
    assert(req.socket.authorized);
    server.close();
  });
  req.on('error', function(err) {
    throw err;
  });
  req.end();
}

process.on('exit', function() {
  assert.equal(reqCount, 1);
});
