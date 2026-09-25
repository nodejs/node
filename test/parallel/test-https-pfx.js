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

const fixtures = require('../common/fixtures');

const assert = require('assert');
const https = require('https');
const { hasFIPS } = require('../common/crypto');
const fips3 = hasFIPS(3);

const pfx = fixtures.readKey('rsa_cert.pfx');

const options = {
  host: '127.0.0.1',
  port: undefined,
  path: '/',
  pfx: pfx,
  passphrase: 'sample',
  requestCert: true,
  rejectUnauthorized: false
};

if (fips3) {
  assert.throws(() => https.createServer(options), {
    code: 'ERR_CRYPTO_UNSUPPORTED_OPERATION',
  });

  if (!hasFIPS(3, 5)) {
    return;
  }

  options.pfx = fixtures.readKey('agent1-fips.pfx');
  options.passphrase = 'password';
}

const server = https.createServer(options, common.mustCallAtLeast((req, res) => {
  assert.strictEqual(req.socket.authorized, fips3);
  assert.strictEqual(req.socket.authorizationError, fips3 ?
    null : 'DEPTH_ZERO_SELF_SIGNED_CERT');
  res.writeHead(200);
  res.end('OK');
}));

server.listen(0, options.host, common.mustCall(function() {
  options.port = this.address().port;

  https.get(options, common.mustCall(function(res) {
    let data = '';

    res.on('data', function(data_) { data += data_; });
    res.on('end', common.mustCall(function() {
      assert.strictEqual(data, 'OK');
      server.close();
    }));
  }));
}));
