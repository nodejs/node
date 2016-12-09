'use strict';
var common = require('../common');
var assert = require('assert');

if (!common.hasCrypto) {
  common.skip('missing crypto');
  return;
}
var https = require('https');

var fs = require('fs');
var path = require('path');

var options = {
  key: fs.readFileSync(path.join(common.fixturesDir, 'test_key.pem')),
  cert: fs.readFileSync(path.join(common.fixturesDir, 'test_cert.pem'))
};

var server = https.createServer(options, common.mustCall(function(req, res) {
  res.writeHead(200);
  res.end();
  req.resume();
}, 2)).listen(0, function() {
  unauthorized();
});

function unauthorized() {
  var req = https.request({
    port: server.address().port,
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
    port: server.address().port
  };
  options.agent = new https.Agent(options);
  var req = https.request(options, common.fail);
  req.on('error', function(err) {
    authorized();
  });
  req.end();
}

function authorized() {
  var options = {
    port: server.address().port,
    ca: [fs.readFileSync(path.join(common.fixturesDir, 'test_cert.pem'))]
  };
  options.agent = new https.Agent(options);
  var req = https.request(options, function(res) {
    res.resume();
    assert(req.socket.authorized);
    server.close();
  });
  req.on('error', common.fail);
  req.end();
}
