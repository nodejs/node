'use strict';
const common = require('../common');

if (!process.features.tls_ocsp)
  common.skip('node compiled without OpenSSL or with old OpenSSL version.');

if (!common.opensslCli)
  common.skip('node compiled without OpenSSL CLI.');

if (!common.hasCrypto)
  common.skip('missing crypto');

const tls = require('tls');
const fixtures = require('../common/fixtures');

const assert = require('assert');

const SSL_OP_NO_TICKET = require('crypto').constants.SSL_OP_NO_TICKET;

const pfx = fixtures.readKey('agent1-pfx.pem');

function test(testOptions, cb) {

  const key = fixtures.readKey('agent1-key.pem');
  const cert = fixtures.readKey('agent1-cert.pem');
  const ca = fixtures.readKey('ca1-cert.pem');
  const options = {
    key,
    cert,
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
