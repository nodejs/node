'use strict';
const common = require('../common');
if (!common.hasCrypto)
  common.skip('missing crypto');
const fixtures = require('../common/fixtures');

const assert = require('assert');
const tls = require('tls');

const key = fixtures.readKey('agent2-key.pem');
const cert = fixtures.readKey('agent2-cert.pem');

let ntests = 0;
let nsuccess = 0;

function loadDHParam(n) {
  return fixtures.readKey(`dh${n}.pem`);
}

const cipherlist = {
  'NOT_PFS': 'AES128-SHA256',
  'DH': 'DHE-RSA-AES128-GCM-SHA256',
  'ECDH': 'ECDHE-RSA-AES128-GCM-SHA256'
};

function test(size, type, name, next) {
  const cipher = type ? cipherlist[type] : cipherlist['NOT_PFS'];

  if (name) tls.DEFAULT_ECDH_CURVE = name;

  const options = {
    key: key,
    cert: cert,
    ciphers: cipher
  };

  if (type === 'DH') options.dhparam = loadDHParam(size);

  const server = tls.createServer(options, function(conn) {
    assert.strictEqual(conn.getEphemeralKeyInfo(), null);
    conn.end();
  });

  server.on('close', common.mustCall(function(err) {
    assert.ifError(err);
    if (next) next();
  }));

  server.listen(0, '127.0.0.1', common.mustCall(function() {
    const client = tls.connect({
      port: this.address().port,
      rejectUnauthorized: false
    }, function() {
      const ekeyinfo = client.getEphemeralKeyInfo();
      assert.strictEqual(ekeyinfo.type, type);
      assert.strictEqual(ekeyinfo.size, size);
      assert.strictEqual(ekeyinfo.name, name);
      nsuccess++;
      server.close();
    });
  }));
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
  test(256, 'ECDH', 'prime256v1', testECDHE512);
  ntests++;
}

function testECDHE512() {
  test(521, 'ECDH', 'secp521r1', null);
  ntests++;
}

testNOT_PFS();

process.on('exit', function() {
  assert.strictEqual(ntests, nsuccess);
  assert.strictEqual(ntests, 5);
});
