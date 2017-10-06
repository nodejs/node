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
if (!common.hasCrypto)
  common.skip('missing crypto');

const assert = require('assert');
const tls = require('tls');
const fs = require('fs');

const options = {
  key: [
    fs.readFileSync(`${common.fixturesDir}/keys/ec-key.pem`),
    fs.readFileSync(`${common.fixturesDir}/keys/agent1-key.pem`),
  ],
  cert: [
    fs.readFileSync(`${common.fixturesDir}/keys/agent1-cert.pem`),
    fs.readFileSync(`${common.fixturesDir}/keys/ec-cert.pem`)
  ]
};

const ciphers = [];

const server = tls.createServer(options, function(conn) {
  conn.end('ok');
}).listen(0, function() {
  const ecdsa = tls.connect(this.address().port, {
    ciphers: 'ECDHE-ECDSA-AES256-GCM-SHA384',
    rejectUnauthorized: false
  }, function() {
    ciphers.push(ecdsa.getCipher());
    const rsa = tls.connect(server.address().port, {
      ciphers: 'ECDHE-RSA-AES256-GCM-SHA384',
      rejectUnauthorized: false
    }, function() {
      ciphers.push(rsa.getCipher());
      ecdsa.end();
      rsa.end();
      server.close();
    });
  });
});

process.on('exit', function() {
  assert.deepStrictEqual(ciphers, [{
    name: 'ECDHE-ECDSA-AES256-GCM-SHA384',
    version: 'TLSv1/SSLv3'
  }, {
    name: 'ECDHE-RSA-AES256-GCM-SHA384',
    version: 'TLSv1/SSLv3'
  }]);
});
