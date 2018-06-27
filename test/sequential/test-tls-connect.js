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
const tls = require('tls');

// https://github.com/joyent/node/issues/1218
// uncatchable exception on TLS connection error
{
  const cert = fixtures.readSync('test_cert.pem');
  const key = fixtures.readSync('test_key.pem');

  const options = { cert: cert, key: key, port: common.PORT };
  const conn = tls.connect(options, common.mustNotCall());

  conn.on(
    'error',
    common.mustCall((e) => { assert.strictEqual(e.code, 'ECONNREFUSED'); })
  );
}

// SSL_accept/SSL_connect error handling
{
  const cert = fixtures.readSync('test_cert.pem');
  const key = fixtures.readSync('test_key.pem');

  assert.throws(() => {
    tls.connect({
      cert: cert,
      key: key,
      port: common.PORT,
      ciphers: 'rick-128-roll'
    }, common.mustNotCall());
  }, /no cipher match/i);
}
