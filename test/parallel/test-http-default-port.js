'use strict';
const common = require('../common');

if (!common.hasCrypto)
  common.skip('missing crypto');

const fixtures = require('../common/fixtures');
const http = require('http');
const assert = require('assert');
const hostExpect = 'localhost';
const options = {
  key: fixtures.readKey('agent1-key.pem'),
  cert: fixtures.readKey('agent1-cert.pem')
};
let gotHttpsResp = false;
let gotHttpResp = false;

let https;
if (common.hasCrypto) {
  https = require('https');
} else {
  common.printSkipMessage('missing crypto');
}

process.on('exit', function() {
  if (common.hasCrypto) {
    assert(gotHttpsResp);
  }
  assert(gotHttpResp);
  console.log('ok');
});

http.createServer(function(req, res) {
  assert.strictEqual(req.headers.host, hostExpect);
  assert.strictEqual(req.headers['x-port'], this.address().port.toString());
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
    assert.strictEqual(req.headers.host, hostExpect);
    assert.strictEqual(req.headers['x-port'], this.address().port.toString());
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
