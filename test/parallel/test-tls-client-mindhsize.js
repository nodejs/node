'use strict';
const common = require('../common');
if (!common.hasCrypto)
  common.skip('missing crypto');

const assert = require('assert');
const tls = require('tls');
const fixtures = require('../common/fixtures');

const key = fixtures.readKey('agent2-key.pem');
const cert = fixtures.readKey('agent2-cert.pem');

let nsuccess = 0;
let nerror = 0;

function loadDHParam(n) {
  return fixtures.readKey(`dh${n}.pem`);
}

function test(size, err, next) {
  const options = {
    key: key,
    cert: cert,
    dhparam: loadDHParam(size),
    ciphers: 'DHE-RSA-AES128-GCM-SHA256'
  };

  const server = tls.createServer(options, function(conn) {
    conn.end();
  });

  server.on('close', function(isException) {
    assert(!isException);
    if (next) next();
  });

  server.listen(0, '127.0.0.1', function() {
    // Client set minimum DH parameter size to 2048 bits so that
    // it fails when it make a connection to the tls server where
    // dhparams is 1024 bits
    const client = tls.connect({
      minDHSize: 2048,
      port: this.address().port,
      rejectUnauthorized: false
    }, function() {
      nsuccess++;
      server.close();
    });
    if (err) {
      client.on('error', function(e) {
        nerror++;
        assert.strictEqual(e.code, 'ERR_TLS_DH_PARAM_SIZE');
        server.close();
      });
    }
  });
}

// A client connection fails with an error when a client has an
// 2048 bits minDHSize option and a server has 1024 bits dhparam
function testDHE1024() {
  test(1024, true, testDHE2048);
}

// A client connection successes when a client has an
// 2048 bits minDHSize option and a server has 2048 bits dhparam
function testDHE2048() {
  test(2048, false, null);
}

testDHE1024();

assert.throws(() => test(512, true, common.mustNotCall()),
              /DH parameter is less than 1024 bits/);

let errMessage = /minDHSize is not a positive number/;
[0, -1, -Infinity, NaN].forEach((minDHSize) => {
  assert.throws(() => tls.connect({ minDHSize }),
                errMessage);
});

errMessage = /minDHSize is not a number/;
[true, false, null, undefined, {}, [], '', '1'].forEach((minDHSize) => {
  assert.throws(() => tls.connect({ minDHSize }), errMessage);
});

process.on('exit', function() {
  assert.strictEqual(nsuccess, 1);
  assert.strictEqual(nerror, 1);
});
