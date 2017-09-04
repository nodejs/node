'use strict';
const common = require('../common');

// This test ensures that the value "auto" on ecdhCurve option is
// supported to enable automatic curve selection in TLS server.

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
  ecdhCurve: 'auto'
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
});
