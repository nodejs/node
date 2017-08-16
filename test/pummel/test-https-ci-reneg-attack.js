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
const fs = require('fs');

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
    cert: fs.readFileSync(`${common.fixturesDir}/test_cert.pem`),
    key: fs.readFileSync(`${common.fixturesDir}/test_key.pem`)
  };

  let seenError = false;

  const server = https.createServer(options, function(req, res) {
    const conn = req.connection;
    conn.on('error', function(err) {
      console.error(`Caught exception: ${err}`);
      assert(/TLS session renegotiation attack/.test(err));
      conn.destroy();
      seenError = true;
    });
    res.end('ok');
  });

  server.listen(common.PORT, function() {
    const args = (`s_client -connect 127.0.0.1:${common.PORT}`).split(' ');
    const child = spawn(common.opensslCli, args);

    //child.stdout.pipe(process.stdout);
    //child.stderr.pipe(process.stderr);

    child.stdout.resume();
    child.stderr.resume();

    // count handshakes, start the attack after the initial handshake is done
    let handshakes = 0;
    let renegs = 0;

    child.stderr.on('data', function(data) {
      if (seenError) return;
      handshakes += ((String(data)).match(/verify return:1/g) || []).length;
      if (handshakes === 2) spam();
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
      child.stdin.write('R\n');
      setTimeout(spam, 50);
    }
  });
}
