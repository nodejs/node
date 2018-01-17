'use strict';
const common = require('../common');

// This test ensures that ecdhCurve option of TLS server supports colon
// separated ECDH curve names as value.

if (!common.hasCrypto)
  common.skip('missing crypto');

if (!common.opensslCli)
  common.skip('missing openssl-cli');

const assert = require('assert');
const tls = require('tls');
const spawn = require('child_process').spawn;
const fixtures = require('../common/fixtures');

function loadPEM(n) {
  return fixtures.readKey(`${n}.pem`);
}

const options = {
  key: loadPEM('agent2-key'),
  cert: loadPEM('agent2-cert'),
  ciphers: '-ALL:ECDHE-RSA-AES128-SHA256',
  ecdhCurve: 'secp256k1:prime256v1:secp521r1'
};

const reply = 'I AM THE WALRUS'; // something recognizable

const server = tls.createServer(options, function(conn) {
  conn.end(reply);
});

let gotReply = false;

server.listen(0, function() {
  const args = ['s_client',
                '-cipher', `${options.ciphers}`,
                '-connect', `127.0.0.1:${this.address().port}`];

  // for the performance and stability issue in s_client on Windows
  if (common.isWindows)
    args.push('-no_rand_screen');

  const client = spawn(common.opensslCli, args);

  client.stdout.on('data', function(data) {
    const message = data.toString();
    if (message.includes(reply))
      gotReply = true;
  });

  client.on('exit', function(code) {
    assert.strictEqual(0, code);
    server.close();
  });

  client.on('error', assert.ifError);
});

process.on('exit', function() {
  assert.ok(gotReply);

  // Some of unsupported curves
  const unsupportedCurves = [
    'wap-wsg-idm-ecid-wtls1',
    'c2pnb163v1',
    'prime192v3'
  ];

  // Brainpool is not supported in FIPS mode
  if (common.hasFipsCrypto)
    unsupportedCurves.push('brainpoolP256r1');

  unsupportedCurves.forEach((ecdhCurve) => {
    assert.throws(() => tls.createServer({ ecdhCurve }),
                  /Error: Failed to set ECDH curve/);
  });
});
