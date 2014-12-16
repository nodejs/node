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

if (!common.opensslCli) {
  console.error('Skipping because node compiled without OpenSSL CLI.');
  process.exit(0);
}

var assert = require('assert');
var spawn = require('child_process').spawn;
var tls = require('tls');
var fs = require('fs');
var key =  fs.readFileSync(common.fixturesDir + '/keys/agent2-key.pem');
var cert = fs.readFileSync(common.fixturesDir + '/keys/agent2-cert.pem');
var nsuccess = 0;
var ntests = 0;
var ciphers = 'DHE-RSA-AES128-SHA256:ECDHE-RSA-AES128-SHA256';


function loadDHParam(n) {
  var path = common.fixturesDir;
  if (n !== 'error') path += '/keys';
  return fs.readFileSync(path + '/dh' + n + '.pem');
}

function test(keylen, expectedCipher, cb) {
  var options = {
    key: key,
    cert: cert,
    dhparam: loadDHParam(keylen)
  };

  var server = tls.createServer(options, function(conn) {
    conn.end();
  });

  server.on('close', function(err) {
    assert(!err);
    if (cb) cb();
  });

  server.listen(common.PORT, '127.0.0.1', function() {
    var args = ['s_client', '-connect', '127.0.0.1:' + common.PORT,
                '-cipher', ciphers];
    var client = spawn(common.opensslCli, args);
    var out = '';
    client.stdout.setEncoding('utf8');
    client.stdout.on('data', function(d) {
      out += d;
    });
    client.stdout.on('end', function() {
      // DHE key length can be checked -brief option in s_client but it
      // is only supported in openssl 1.0.2 so we cannot check it.
      var reg = new RegExp('Cipher    : ' + expectedCipher);
      if (reg.test(out)) {
        nsuccess++;
        server.close();
      }
    });
  });
}

function test512() {
  test(512, 'DHE-RSA-AES128-SHA256', test1024);
  ntests++;
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
  test('error', 'ECDHE-RSA-AES128-SHA256', null);
  ntests++;
}

test512();

process.on('exit', function() {
  assert.equal(ntests, nsuccess);
});
