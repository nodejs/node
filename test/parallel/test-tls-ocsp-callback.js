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
}

const crypto = require('crypto');
const tls = require('tls');
const fixtures = require('../common/fixtures');
const { hasOpenSSL } = require('../common/crypto');

const assert = require('assert');

const SSL_OP_NO_TICKET = require('crypto').constants.SSL_OP_NO_TICKET;

const pfx = fixtures.readKey('agent1.pfx');
const key = fixtures.readKey('agent1-key.pem');
const cert = fixtures.readKey('agent1-cert.pem');
const ca = fixtures.readKey('ca1-cert.pem');

function test(testOptions, cb) {
  const options = {
    key,
    cert,
    ca: [ca]
  };
  const requestCount = testOptions.response ? 0 : 1;

  if (!testOptions.ocsp)
    assert.strictEqual(testOptions.response, undefined);

  if (testOptions.pfx) {
    delete options.key;
    delete options.cert;
    options.pfx = testOptions.pfx;
    options.passphrase = testOptions.passphrase;
  }

  const server = tls.createServer(options, common.mustCall((cleartext) => {
    cleartext.on('error', function(er) {
      // We're ok with getting ECONNRESET in this test, but it's
      // timing-dependent, and thus unreliable. Any other errors
      // are just failures, though.
      if (er.code !== 'ECONNRESET')
        throw er;
    });
    cleartext.end();
  }, requestCount));

  if (!testOptions.ocsp)
    server.on('OCSPRequest', common.mustNotCall());
  else
    server.on('OCSPRequest', common.mustCall((cert, issuer, callback) => {
      assert.ok(Buffer.isBuffer(cert));
      assert.ok(Buffer.isBuffer(issuer));

      // Callback a little later to ensure that async really works.
      return setTimeout(callback, 100, null, testOptions.response ?
        Buffer.from(testOptions.response) : null);
    }));

  server.listen(0, common.mustCall(function() {
    const client = tls.connect({
      port: this.address().port,
      requestOCSP: testOptions.ocsp,
      secureOptions: testOptions.ocsp ? 0 : SSL_OP_NO_TICKET,
      rejectUnauthorized: false
    }, common.mustCall(requestCount));

    client.on('OCSPResponse', common.mustCall((resp) => {
      if (testOptions.response) {
        if (Buffer.isBuffer(testOptions.response))
          assert.deepStrictEqual(resp, testOptions.response);
        else
          assert.strictEqual(resp.toString(), testOptions.response);
        client.destroy();
      } else {
        assert.strictEqual(resp, null);
      }
    }, testOptions.ocsp === false ? 0 : 1));

    client.on('close', common.mustCall(() => {
      server.close(cb);
    }));
  }));
}

// OpenSSL 3.6+ validates that the value passed to
// SSL_set_tlsext_status_ocsp_resp parses as DER, so the test responses need
// to be valid DER-encoded OCSPResponse values.
// Minimal OCSPResponse is SEQUENCE { ENUMERATED responseStatus } where
// 0 = successful and 1 = malformedRequest.
const response1 = Buffer.from([0x30, 0x03, 0x0a, 0x01, 0x00]);
const response2 = Buffer.from([0x30, 0x03, 0x0a, 0x01, 0x01]);

test({ ocsp: true, response: false });
test({ ocsp: true, response: response1 });
test({ ocsp: false });

if (!crypto.getFips()) {
  test({ ocsp: true, response: response2, pfx: pfx, passphrase: 'sample' });
}

// Older OpenSSL versions accept arbitrary bytes (not just DER) as the OCSP
// response, so additionally exercise the string path there.
if (!hasOpenSSL(3, 6)) {
  test({ ocsp: true, response: 'hello world' });
  if (!crypto.getFips()) {
    test({ ocsp: true, response: 'hello pfx', pfx: pfx, passphrase: 'sample' });
  }
}
