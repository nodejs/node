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

if (!process.features.tls_ocsp) {
  common.skip('node compiled without OpenSSL or ' +
              'with old OpenSSL version.');
  return;
}
if (!common.opensslCli) {
  common.skip('node compiled without OpenSSL CLI.');
  return;
}

if (!common.hasCrypto) {
  common.skip('missing crypto');
  return;
}
const tls = require('tls');

const assert = require('assert');
const fs = require('fs');
const join = require('path').join;

const SSL_OP_NO_TICKET = require('crypto').constants.SSL_OP_NO_TICKET;

const pfx = fs.readFileSync(join(common.fixturesDir, 'keys', 'agent1-pfx.pem'));

function test(testOptions, cb) {

  const keyFile = join(common.fixturesDir, 'keys', 'agent1-key.pem');
  const certFile = join(common.fixturesDir, 'keys', 'agent1-cert.pem');
  const caFile = join(common.fixturesDir, 'keys', 'ca1-cert.pem');
  const key = fs.readFileSync(keyFile);
  const cert = fs.readFileSync(certFile);
  const ca = fs.readFileSync(caFile);
  const options = {
    key: key,
    cert: cert,
    ca: [ca]
  };
  let requestCount = 0;
  let clientSecure = 0;
  let ocspCount = 0;
  let ocspResponse;

  if (testOptions.pfx) {
    delete options.key;
    delete options.cert;
    options.pfx = testOptions.pfx;
    options.passphrase = testOptions.passphrase;
  }

  const server = tls.createServer(options, function(cleartext) {
    cleartext.on('error', function(er) {
      // We're ok with getting ECONNRESET in this test, but it's
      // timing-dependent, and thus unreliable. Any other errors
      // are just failures, though.
      if (er.code !== 'ECONNRESET')
        throw er;
    });
    ++requestCount;
    cleartext.end();
  });
  server.on('OCSPRequest', function(cert, issuer, callback) {
    ++ocspCount;
    assert.ok(Buffer.isBuffer(cert));
    assert.ok(Buffer.isBuffer(issuer));

    // Just to check that async really works there
    setTimeout(function() {
      callback(null,
               testOptions.response ? Buffer.from(testOptions.response) : null);
    }, 100);
  });
  server.listen(0, function() {
    const client = tls.connect({
      port: this.address().port,
      requestOCSP: testOptions.ocsp !== false,
      secureOptions: testOptions.ocsp === false ?
          SSL_OP_NO_TICKET : 0,
      rejectUnauthorized: false
    }, function() {
      clientSecure++;
    });
    client.on('OCSPResponse', function(resp) {
      ocspResponse = resp;
      if (resp)
        client.destroy();
    });
    client.on('close', function() {
      server.close(cb);
    });
  });

  process.on('exit', function() {
    if (testOptions.ocsp === false) {
      assert.strictEqual(requestCount, clientSecure);
      assert.strictEqual(requestCount, 1);
      return;
    }

    if (testOptions.response) {
      assert.strictEqual(ocspResponse.toString(), testOptions.response);
    } else {
      assert.strictEqual(ocspResponse, null);
    }
    assert.strictEqual(requestCount, testOptions.response ? 0 : 1);
    assert.strictEqual(clientSecure, requestCount);
    assert.strictEqual(ocspCount, 1);
  });
}

const tests = [
  { response: false },
  { response: 'hello world' },
  { ocsp: false }
];

if (!common.hasFipsCrypto) {
  tests.push({ pfx: pfx, passphrase: 'sample', response: 'hello pfx' });
}

function runTests(i) {
  if (i === tests.length) return;

  test(tests[i], common.mustCall(function() {
    runTests(i + 1);
  }));
}

runTests(0);
