'use strict';
const common = require('../common');
const http = require('http');
const assert = require('assert');
const hostExpect = 'localhost';
const fs = require('fs');
const path = require('path');
const fixtures = path.join(common.fixturesDir, 'keys');
const options = {
  key: fs.readFileSync(fixtures + '/agent1-key.pem'),
  cert: fs.readFileSync(fixtures + '/agent1-cert.pem')
};
let gotHttpsResp = false;
let gotHttpResp = false;

if (common.hasCrypto) {
  var https = require('https');
} else {
  common.skip('missing crypto');
}

process.on('exit', function() {
  if (common.hasCrypto) {
    assert(gotHttpsResp);
  }
  assert(gotHttpResp);
  console.log('ok');
});

http.createServer(function(req, res) {
  assert.equal(req.headers.host, hostExpect);
  assert.equal(req.headers['x-port'], this.address().port);
  res.writeHead(200);
  res.end('ok');
  this.close();
}).listen(0, function() {
  http.globalAgent.defaultPort = this.address().port;
  http.get({
    host: 'localhost',
    headers: {
      'x-port': this.address().port
    }
  }, function(res) {
    gotHttpResp = true;
    res.resume();
  });
});

if (common.hasCrypto) {
  https.createServer(options, function(req, res) {
    assert.equal(req.headers.host, hostExpect);
    assert.equal(req.headers['x-port'], this.address().port);
    res.writeHead(200);
    res.end('ok');
    this.close();
  }).listen(0, function() {
    https.globalAgent.defaultPort = this.address().port;
    https.get({
      host: 'localhost',
      rejectUnauthorized: false,
      headers: {
        'x-port': this.address().port
      }
    }, function(res) {
      gotHttpsResp = true;
      res.resume();
    });
  });
}
