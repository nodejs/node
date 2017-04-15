// Flags: --no-warnings
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
const assert = require('assert');

if (!common.hasCrypto) {
  common.skip('missing crypto');
  return;
}

if (!common.opensslCli) {
  common.skip('missing openssl-cli');
  return;
}

const tls = require('tls');

const spawn = require('child_process').spawn;
const fs = require('fs');
const key = fs.readFileSync(common.fixturesDir + '/keys/agent2-key.pem');
const cert = fs.readFileSync(common.fixturesDir + '/keys/agent2-cert.pem');
let nsuccess = 0;
let ntests = 0;
const ciphers = 'DHE-RSA-AES128-SHA256:ECDHE-RSA-AES128-SHA256';

// Test will emit a warning because the DH parameter size is < 2048 bits
common.expectWarning('SecurityWarning',
                     'DH parameter is less than 2048 bits');

function loadDHParam(n) {
  let path = common.fixturesDir;
  if (n !== 'error') path += '/keys';
  return fs.readFileSync(path + '/dh' + n + '.pem');
}

function test(keylen, expectedCipher, cb) {
  const options = {
    key: key,
    cert: cert,
    ciphers: ciphers,
    dhparam: loadDHParam(keylen)
  };

  const server = tls.createServer(options, function(conn) {
    conn.end();
  });

  server.on('close', function(err) {
    assert.ifError(err);
    if (cb) cb();
  });

  server.listen(0, '127.0.0.1', function() {
    const args = ['s_client', '-connect', `127.0.0.1:${this.address().port}`,
                  '-cipher', ciphers];

    // for the performance and stability issue in s_client on Windows
    if (common.isWindows)
      args.push('-no_rand_screen');

    const client = spawn(common.opensslCli, args);
    let out = '';
    client.stdout.setEncoding('utf8');
    client.stdout.on('data', function(d) {
      out += d;
    });
    client.stdout.on('end', function() {
      // DHE key length can be checked -brief option in s_client but it
      // is only supported in openssl 1.0.2 so we cannot check it.
      const reg = new RegExp('Cipher    : ' + expectedCipher);
      if (reg.test(out)) {
        nsuccess++;
        server.close();
      }
    });
  });
}

function test512() {
  assert.throws(function() {
    test(512, 'DHE-RSA-AES128-SHA256', null);
  }, /DH parameter is less than 1024 bits/);
}

function test1024() {
  test(1024, 'DHE-RSA-AES128-SHA256', test2048);
  ntests++;
}

function test2048() {
  test(2048, 'DHE-RSA-AES128-SHA256', testError);
  ntests++;
}

function testError() {
  test('error', 'ECDHE-RSA-AES128-SHA256', test512);
  ntests++;
}

test1024();

process.on('exit', function() {
  assert.strictEqual(ntests, nsuccess);
  assert.strictEqual(ntests, 3);
});
