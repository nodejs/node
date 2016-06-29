'use strict';
var common = require('../common');
var assert = require('assert');

if (!common.hasCrypto) {
  common.skip('missing crypto');
  return;
}
var tls = require('tls');

var spawn = require('child_process').spawn;
var fs = require('fs');
var key = fs.readFileSync(common.fixturesDir + '/keys/agent2-key.pem');
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
    ciphers: ciphers,
    dhparam: loadDHParam(keylen)
  };

  var server = tls.createServer(options, function(conn) {
    conn.end();
  });

  server.on('close', function(err) {
    assert(!err);
    if (cb) cb();
  });

  server.listen(0, '127.0.0.1', function() {
    var args = ['s_client', '-connect', `127.0.0.1:${this.address().port}`,
                '-cipher', ciphers];

    // for the performance and stability issue in s_client on Windows
    if (common.isWindows)
      args.push('-no_rand_screen');

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
  assert.equal(ntests, nsuccess);
  assert.equal(ntests, 3);
});
