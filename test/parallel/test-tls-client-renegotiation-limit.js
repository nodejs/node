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

if (!common.opensslCli)
  common.skip('node compiled without OpenSSL CLI.');

const assert = require('assert');
const tls = require('tls');
const fixtures = require('../common/fixtures');

// renegotiation limits to test
const LIMITS = [0, 1, 2, 3, 5, 10, 16];

{
  let n = 0;
  function next() {
    if (n >= LIMITS.length) return;
    tls.CLIENT_RENEG_LIMIT = LIMITS[n++];
    test(next);
  }
  next();
}

function test(next) {
  const options = {
    cert: fixtures.readSync('test_cert.pem'),
    key: fixtures.readSync('test_key.pem'),
  };

  const server = tls.createServer(options, (conn) => {
    conn.on('error', (err) => {
      console.error(`Caught exception: ${err}`);
      assert(/TLS session renegotiation attack/.test(err));
      conn.destroy();
    });
    conn.pipe(conn);
  });

  server.listen(0, () => {
    const options = {
      host: server.address().host,
      port: server.address().port,
      rejectUnauthorized: false,
    };
    const client = tls.connect(options, spam);

    let renegs = 0;

    client.on('close', () => {
      assert.strictEqual(renegs, tls.CLIENT_RENEG_LIMIT + 1);
      server.close();
      process.nextTick(next);
    });

    client.on('error', (err) => {
      console.log('CLIENT ERR', err);
      throw err;
    });

    client.on('close', (hadErr) => {
      assert.strictEqual(hadErr, false);
    });

    // simulate renegotiation attack
    function spam() {
      client.write('');
      client.renegotiate({}, (err) => {
        assert.ifError(err);
        assert.ok(renegs <= tls.CLIENT_RENEG_LIMIT);
        spam();
      });
      renegs++;
    }
  });
}
