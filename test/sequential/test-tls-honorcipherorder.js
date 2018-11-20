var common = require('../common');
var assert = require('assert');

if (!common.hasCrypto) {
  console.log('1..0 # Skipped: missing crypto');
  process.exit();
}
var tls = require('tls');

var fs = require('fs');
var nconns = 0;
// test only in TLSv1 to use DES which is no longer supported TLSv1.2
// to be safe when the default method is updated in the future
var SSL_Method = 'TLSv1_method';
var localhost = '127.0.0.1';

process.on('exit', function() {
  assert.equal(nconns, 6);
});

function test(honorCipherOrder, clientCipher, expectedCipher, cb) {
  var soptions = {
    secureProtocol: SSL_Method,
    key: fs.readFileSync(common.fixturesDir + '/keys/agent2-key.pem'),
    cert: fs.readFileSync(common.fixturesDir + '/keys/agent2-cert.pem'),
    ciphers: 'DES-CBC-SHA:AES256-SHA:RC4-SHA:ECDHE-RSA-AES256-SHA',
    honorCipherOrder: !!honorCipherOrder
  };

  var server = tls.createServer(soptions, function(cleartextStream) {
    nconns++;

    // End socket to send CLOSE_NOTIFY and TCP FIN packet, otherwise
    // it may hang for ~30 seconds in FIN_WAIT_1 state (at least on OSX).
    cleartextStream.end();
  });
  server.listen(common.PORT, localhost, function() {
    var coptions = {
      rejectUnauthorized: false,
      secureProtocol: SSL_Method
    };
    if (clientCipher) {
      coptions.ciphers = clientCipher;
    }
    var client = tls.connect(common.PORT, localhost, coptions, function() {
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
  test(false, 'AES256-SHA:DES-CBC-SHA:RC4-SHA','AES256-SHA', test2);
}

function test2() {
  // Server has the preference of cipher suites where DES-CBC-SHA is in
  // the first.
  test(true, 'AES256-SHA:DES-CBC-SHA:RC4-SHA', 'DES-CBC-SHA', test3);
}

function test3() {
  // Server has the preference of cipher suites. RC4-SHA is given
  // higher priority over DES-CBC-SHA among client cipher suites.
  test(true, 'RC4-SHA:AES256-SHA', 'AES256-SHA', test4);
}

function test4() {
  // As client has only one cipher, server has no choice in regardless
  // of honorCipherOrder.
  test(true, 'RC4-SHA', 'RC4-SHA', test5);
}

function test5() {
  // Client did not explicitly set ciphers. Ensure that client defaults to
  // sane ciphers. Even though server gives top priority to DES-CBC-SHA
  // it should not be negotiated because it's not in default client ciphers.
  test(true, null, 'AES256-SHA', test6);
}

function test6() {
  // Ensure that `tls.DEFAULT_CIPHERS` is used
  SSL_Method = 'TLSv1_2_method';
  tls.DEFAULT_CIPHERS = 'ECDHE-RSA-AES256-SHA';
  test(true, null, 'ECDHE-RSA-AES256-SHA');
}
