// Copyright Joyent, Inc. and other Node contributors.
//
// Permission is hereby granted, free of charge, to any person obtaining a
// copy of this software and associated documentation files (the
// "Software"), to deal in the Software without restriction, including
// without limitation the rights to use, copy, modify, merge, publish,
// distribute, sublicense, and/or sell copies of the Software, and to permit
// persons to whom the Software is furnished to do so, subject to the
// following conditions:
//
// The above copyright notice and this permission notice shall be included
// in all copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
// OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
// MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN
// NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM,
// DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR
// OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE
// USE OR OTHER DEALINGS IN THE SOFTWARE.

'use strict';
const common = require('../common');

if (!common.hasCrypto) {
  common.skip('missing crypto');
  return;
}

const http = require('http');
const https = require('https');
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
