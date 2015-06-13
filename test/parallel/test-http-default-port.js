'use strict';
var common = require('../common');
var http = require('http'),
    PORT = common.PORT,
    SSLPORT = common.PORT + 1,
    assert = require('assert'),
    hostExpect = 'localhost',
    fs = require('fs'),
    path = require('path'),
    fixtures = path.resolve(__dirname, '../fixtures/keys'),
    options = {
      key: fs.readFileSync(fixtures + '/agent1-key.pem'),
      cert: fs.readFileSync(fixtures + '/agent1-cert.pem')
    },
    gotHttpsResp = false,
    gotHttpResp = false;

if (common.hasCrypto) {
  var https = require('https');
} else {
  console.log('1..0 # Skipped: missing crypto');
}

process.on('exit', function() {
  if (common.hasCrypto) {
    assert(gotHttpsResp);
  }
  assert(gotHttpResp);
  console.log('ok');
});

http.globalAgent.defaultPort = PORT;
http.createServer(function(req, res) {
  assert.equal(req.headers.host, hostExpect);
  assert.equal(req.headers['x-port'], PORT);
  res.writeHead(200);
  res.end('ok');
  this.close();
}).listen(PORT, function() {
  http.get({
    host: 'localhost',
    headers: {
      'x-port': PORT
    }
  }, function(res) {
    gotHttpResp = true;
    res.resume();
  });
});

if (common.hasCrypto) {
  https.globalAgent.defaultPort = SSLPORT;
  https.createServer(options, function(req, res) {
    assert.equal(req.headers.host, hostExpect);
    assert.equal(req.headers['x-port'], SSLPORT);
    res.writeHead(200);
    res.end('ok');
    this.close();
  }).listen(SSLPORT, function() {
    var req = https.get({
      host: 'localhost',
      rejectUnauthorized: false,
      headers: {
        'x-port': SSLPORT
      }
    }, function(res) {
      gotHttpsResp = true;
      res.resume();
    });
  });
}
