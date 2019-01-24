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
const spawn = require('child_process').spawn;
const tls = require('tls');
const https = require('https');
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
    key: fixtures.readSync('test_key.pem')
  };

  const server = https.createServer(options, function(req, res) {
    const conn = req.connection;
    conn.on('error', function(err) {
      console.error(`Caught exception: ${err}`);
      assert(/TLS session renegotiation attack/.test(err));
      conn.destroy();
    });
    res.end('ok');
  });

  server.listen(0, function() {
    const cmd = `s_client -connect 127.0.0.1:${server.address().port}`;
    const args = cmd.split(' ');
    const child = spawn(common.opensslCli, args);

    child.stdout.resume();
    child.stderr.resume();

    // Count handshakes, start the attack after the initial handshake is done
    let handshakes = 0;
    let renegs = 0;
    let waitingToSpam = true;

    child.stderr.on('data', function(data) {
      handshakes += ((String(data)).match(/verify return:1/g) || []).length;
      if (handshakes === 2 && waitingToSpam) {
        waitingToSpam = false;
        // See long comment in spam() function.
        child.stdin.write('GET / HTTP/1.1\n');
        spam();
      }
      renegs += ((String(data)).match(/RENEGOTIATING/g) || []).length;
    });

    child.on('exit', function() {
      assert.strictEqual(renegs, tls.CLIENT_RENEG_LIMIT + 1);
      server.close();
      process.nextTick(next);
    });

    let closed = false;
    child.stdin.on('error', function(err) {
      switch (err.code) {
        case 'ECONNRESET':
        case 'EPIPE':
          break;
        default:
          assert.strictEqual(err.code, 'ECONNRESET');
          break;
      }
      closed = true;
    });
    child.stdin.on('close', function() {
      closed = true;
    });

    // simulate renegotiation attack
    function spam() {
      if (closed) return;
      // `R` at the start of a line will cause renegotiation but may (in OpenSSL
      // 1.1.1a, at least) also cause `s_client` to send `R` as a HTTP header,
      // causing the server to send 400 Bad Request and close the connection.
      // By sending a `Referer` header that starts with a capital `R`, OpenSSL
      // 1.1.1.a s_client sends both a valid HTTP header *and* causes a
      // renegotiation. ¯\(ツ)/¯ (But you need to send a valid HTTP verb first,
      // hence the `GET` in the stderr data callback.)
      child.stdin.write('Referer: fhqwhgads\n');
      setTimeout(spam, 50);
    }
  });
}
