'use strict';
var common = require('../common');

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
var tls = require('tls');

var assert = require('assert');
var fs = require('fs');
var join = require('path').join;

const SSL_OP_NO_TICKET = require('crypto').constants.SSL_OP_NO_TICKET;

var pfx = fs.readFileSync(join(common.fixturesDir, 'keys', 'agent1-pfx.pem'));

function test(testOptions, cb) {

  var keyFile = join(common.fixturesDir, 'keys', 'agent1-key.pem');
  var certFile = join(common.fixturesDir, 'keys', 'agent1-cert.pem');
  var caFile = join(common.fixturesDir, 'keys', 'ca1-cert.pem');
  var key = fs.readFileSync(keyFile);
  var cert = fs.readFileSync(certFile);
  var ca = fs.readFileSync(caFile);
  var options = {
    key: key,
    cert: cert,
    ca: [ca]
  };
  var requestCount = 0;
  var clientSecure = 0;
  var ocspCount = 0;
  var ocspResponse;

  if (testOptions.pfx) {
    delete options.key;
    delete options.cert;
    options.pfx = testOptions.pfx;
    options.passphrase = testOptions.passphrase;
  }

  var server = tls.createServer(options, function(cleartext) {
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
    var client = tls.connect({
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
      assert.equal(requestCount, clientSecure);
      assert.equal(requestCount, 1);
      return;
    }

    if (testOptions.response) {
      assert.equal(ocspResponse.toString(), testOptions.response);
    } else {
      assert.ok(ocspResponse === null);
    }
    assert.equal(requestCount, testOptions.response ? 0 : 1);
    assert.equal(clientSecure, requestCount);
    assert.equal(ocspCount, 1);
  });
}

var tests = [
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
