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
  key: fs.readFileSync(path.join(common.fixturesDir, 'test_key.pem')),
  cert: fs.readFileSync(path.join(common.fixturesDir, 'test_cert.pem'))
};

var reqCount = 0;

var server = https.createServer(options, function(req, res) {
  ++reqCount;
  res.writeHead(200);
  res.end();
  req.resume();
}).listen(common.PORT, function() {
  unauthorized();
});

function unauthorized() {
  var req = https.request({
    port: common.PORT,
    rejectUnauthorized: false
  }, function(res) {
    assert(!req.socket.authorized);
    res.resume();
    rejectUnauthorized();
  });
  req.on('error', function(err) {
    throw err;
  });
  req.end();
}

function rejectUnauthorized() {
  var options = {
    port: common.PORT
  };
  options.agent = new https.Agent(options);
  var req = https.request(options, function(res) {
    assert(false);
  });
  req.on('error', function(err) {
    authorized();
  });
  req.end();
}

function authorized() {
  var options = {
    port: common.PORT,
    ca: [fs.readFileSync(path.join(common.fixturesDir, 'test_cert.pem'))]
  };
  options.agent = new https.Agent(options);
  var req = https.request(options, function(res) {
    res.resume();
    assert(req.socket.authorized);
    server.close();
  });
  req.on('error', function(err) {
    assert(false);
  });
  req.end();
}

process.on('exit', function() {
  assert.equal(reqCount, 2);
});
