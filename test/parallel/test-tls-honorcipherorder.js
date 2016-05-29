'use strict';
var common = require('../common');
var assert = require('assert');

if (!common.hasCrypto) {
  common.skip('missing crypto');
  return;
}
var tls = require('tls');

var fs = require('fs');
var nconns = 0;

// We explicitly set TLS version to 1.2 so as to be safe when the
// default method is updated in the future
var SSL_Method = 'TLSv1_2_method';
var localhost = '127.0.0.1';

process.on('exit', function() {
  assert.equal(nconns, 6);
});

function test(honorCipherOrder, clientCipher, expectedCipher, cb) {
  var soptions = {
    secureProtocol: SSL_Method,
    key: fs.readFileSync(common.fixturesDir + '/keys/agent2-key.pem'),
    cert: fs.readFileSync(common.fixturesDir + '/keys/agent2-cert.pem'),
    ciphers: 'AES256-SHA256:AES128-GCM-SHA256:AES128-SHA256:' +
             'ECDHE-RSA-AES128-GCM-SHA256',
    honorCipherOrder: !!honorCipherOrder
  };

  var server = tls.createServer(soptions, function(cleartextStream) {
    nconns++;

    // End socket to send CLOSE_NOTIFY and TCP FIN packet, otherwise
    // it may hang for ~30 seconds in FIN_WAIT_1 state (at least on OSX).
    cleartextStream.end();
  });
  server.listen(0, localhost, function() {
    var coptions = {
      rejectUnauthorized: false,
      secureProtocol: SSL_Method
    };
    if (clientCipher) {
      coptions.ciphers = clientCipher;
    }
    const port = this.address().port;
    var client = tls.connect(port, localhost, coptions, function() {
      var cipher = client.getCipher();
      client.end();
      server.close();
      assert.equal(cipher.name, expectedCipher);
      if (cb) cb();
    });
  });
}

test1();

function test1() {
  // Client has the preference of cipher suites by default
  test(false, 'AES128-GCM-SHA256:AES256-SHA256:AES128-SHA256',
       'AES128-GCM-SHA256', test2);
}

function test2() {
  // Server has the preference of cipher suites, and AES256-SHA256 is
  // the server's top choice.
  test(true, 'AES128-GCM-SHA256:AES256-SHA256:AES128-SHA256',
       'AES256-SHA256', test3);
}

function test3() {
  // Server has the preference of cipher suites. AES128-GCM-SHA256 is given
  // higher priority over AES128-SHA256 among client cipher suites.
  test(true, 'AES128-SHA256:AES128-GCM-SHA256', 'AES128-GCM-SHA256', test4);

}

function test4() {
  // As client has only one cipher, server has no choice, irrespective
  // of honorCipherOrder.
  test(true, 'AES128-SHA256', 'AES128-SHA256', test5);
}

function test5() {
  // Client did not explicitly set ciphers and client offers
  // tls.DEFAULT_CIPHERS. All ciphers of the server are included in the
  // default list so the negotiated cipher is selected according to the
  // server's top preference of AES256-SHA256.
  test(true, null, 'AES256-SHA256', test6);
}

function test6() {
  // Ensure that `tls.DEFAULT_CIPHERS` is used
  tls.DEFAULT_CIPHERS = 'ECDHE-RSA-AES128-GCM-SHA256';
  test(true, null, 'ECDHE-RSA-AES128-GCM-SHA256');
}
