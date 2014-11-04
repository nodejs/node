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

var common = require('../common');
var assert = require('assert');
var tls = require('tls');
var fs = require('fs');
var nconns = 0;
var SSL_Method = 'SSLv23_method';
var localhost = '127.0.0.1';
var opCipher = process.binding('constants').SSL_OP_CIPHER_SERVER_PREFERENCE;

/*
 * This test is to make sure we are preserving secureOptions that are passed
 * to the server.
 *
 * Also that if honorCipherOrder is passed we are preserving that in the
 * options.
 *
 * And that if we are passing in secureOptions no new options (aside from the
 * honorCipherOrder case) are added to the secureOptions
 */


process.on('exit', function() {
  assert.equal(nconns, 6);
});

function test(honorCipherOrder, clientCipher, expectedCipher, secureOptions, cb) {
  var soptions = {
    secureProtocol: SSL_Method,
    key: fs.readFileSync(common.fixturesDir + '/keys/agent2-key.pem'),
    cert: fs.readFileSync(common.fixturesDir + '/keys/agent2-cert.pem'),
    ciphers: 'AES256-SHA:RC4-SHA:DES-CBC-SHA',
    secureOptions: secureOptions,
    honorCipherOrder: !!honorCipherOrder
  };

  var server = tls.createServer(soptions, function(cleartextStream) {
    nconns++;
  });

  if (!!honorCipherOrder) {
    assert.strictEqual(server.secureOptions & opCipher, opCipher, 'we should preserve cipher preference');
  }

  if (secureOptions) {
    var expectedSecureOpts = secureOptions;
    if (!!honorCipherOrder) expectedSecureOpts |= opCipher;

    assert.strictEqual(server.secureOptions & expectedSecureOpts,
                       expectedSecureOpts, 'we should preserve secureOptions');
    assert.strictEqual(server.secureOptions & ~expectedSecureOpts,
                       0,
                       'we should not add extra options');
  }

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
  test(false, 'DES-CBC-SHA:RC4-SHA:AES256-SHA','DES-CBC-SHA', 0, test2);
}

function test2() {
  // Server has the preference of cipher suites where AES256-SHA is in
  // the first.
  test(true, 'DES-CBC-SHA:RC4-SHA:AES256-SHA', 'AES256-SHA', 0, test3);
}

function test3() {
  // Server has the preference of cipher suites. RC4-SHA is given
  // higher priority over DES-CBC-SHA among client cipher suites.
  test(true, 'DES-CBC-SHA:RC4-SHA', 'RC4-SHA', 0, test4);
}

function test4() {
  // As client has only one cipher, server has no choice in regardless
  // of honorCipherOrder.
  test(true, 'DES-CBC-SHA', 'DES-CBC-SHA', 0, test5);
}

function test5() {
  test(false,
       'DES-CBC-SHA',
       'DES-CBC-SHA',
       process.binding('constants').SSL_OP_SINGLE_DH_USE, test6);
}

function test6() {
  test(true,
       'DES-CBC-SHA',
       'DES-CBC-SHA',
       process.binding('constants').SSL_OP_SINGLE_DH_USE);
}
