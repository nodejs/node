'use strict';
var common = require('../common');
var assert = require('assert');

if (!common.hasCrypto) {
  common.skip('missing crypto');
  process.exit();
}
var tls = require('tls');

var fs = require('fs');
var key = fs.readFileSync(common.fixturesDir + '/keys/agent2-key.pem');
var cert = fs.readFileSync(common.fixturesDir + '/keys/agent2-cert.pem');

var ntests = 0;
var nsuccess = 0;

function loadDHParam(n) {
  var path = common.fixturesDir;
  if (n !== 'error') path += '/keys';
  return fs.readFileSync(path + '/dh' + n + '.pem');
}

var cipherlist = {
  'NOT_PFS': 'AES128-SHA256',
  'DH': 'DHE-RSA-AES128-GCM-SHA256',
  'ECDH': 'ECDHE-RSA-AES128-GCM-SHA256'
};

function test(size, type, name, next) {
  var cipher = type ? cipherlist[type] : cipherlist['NOT_PFS'];

  if (name) tls.DEFAULT_ECDH_CURVE = name;

  var options = {
    key: key,
    cert: cert,
    ciphers: cipher
  };

  if (type === 'DH') options.dhparam = loadDHParam(size);

  var server = tls.createServer(options, function(conn) {
    assert.strictEqual(conn.getEphemeralKeyInfo(), null);
    conn.end();
  });

  server.on('close', function(err) {
    assert(!err);
    if (next) next();
  });

  server.listen(0, '127.0.0.1', function() {
    var client = tls.connect({
      port: this.address().port,
      rejectUnauthorized: false
    }, function() {
      var ekeyinfo = client.getEphemeralKeyInfo();
      assert.strictEqual(ekeyinfo.type, type);
      assert.strictEqual(ekeyinfo.size, size);
      assert.strictEqual(ekeyinfo.name, name);
      nsuccess++;
      server.close();
    });
  });
}

function testNOT_PFS() {
  test(undefined, undefined, undefined, testDHE1024);
  ntests++;
}

function testDHE1024() {
  test(1024, 'DH', undefined, testDHE2048);
  ntests++;
}

function testDHE2048() {
  test(2048, 'DH', undefined, testECDHE256);
  ntests++;
}

function testECDHE256() {
  test(256, 'ECDH', tls.DEFAULT_ECDH_CURVE, testECDHE512);
  ntests++;
}

function testECDHE512() {
  test(521, 'ECDH', 'secp521r1', null);
  ntests++;
}

testNOT_PFS();

process.on('exit', function() {
  assert.equal(ntests, nsuccess);
  assert.equal(ntests, 5);
});
