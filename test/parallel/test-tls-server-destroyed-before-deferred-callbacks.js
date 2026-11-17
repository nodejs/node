'use strict';

// The server's 'onclienthello' and 'oncertcb' callbacks run from an immediate,
// so a socket can be destroyed after its ClientHello is processed but before
// they run. Destroyed sockets here must be ignored, not crash.

const common = require('../common');
if (!common.hasCrypto)
  common.skip('missing crypto');

const fixtures = require('../common/fixtures');
const assert = require('assert');
const tls = require('tls');
const { once } = require('events');
const { Duplex } = require('stream');

const key = fixtures.readKey('agent1-key.pem');
const cert = fixtures.readKey('agent1-cert.pem');

// A ClientHello with no SNI that requests OCSP stapling.
async function getClientHello() {
  const { promise, resolve } = Promise.withResolvers();
  const client = tls.connect({
    host: '127.0.0.1',
    requestOCSP: true,
    socket: new Duplex({ read() {}, write: resolve }),
  });
  const hello = await promise;
  client.destroy();
  return hello;
}

// Delivers the hello and EOF in the same tick, so the server is destroyed
// before its deferred callbacks run. Like a net.Socket, the connection is not
// half-open and is closed asynchronously rather than auto-destroyed.
async function sendHelloThenClose(server, hello) {
  server.on('secureConnection', common.mustNotCall());
  const connection = new Duplex({
    allowHalfOpen: false,
    autoDestroy: false,
    read() {},
    write(chunk, encoding, callback) { callback(); },
  });
  server.emit('connection', connection);
  connection.push(hello);
  connection.push(null);

  const [err] = await once(server, 'tlsClientError');
  assert.strictEqual(err.code, 'ECONNRESET');
}

(async () => {
  const hello = await getClientHello();

  // Repro via oncertcb reaching requestOCSP():
  await sendHelloThenClose(tls.createServer({
    key, cert,
    SNICallback: common.mustNotCall(),
  }), hello);

  // Repro via 'onclienthello', when session callbacks are enabled:
  const sessionServer = tls.createServer({ key, cert });
  sessionServer.on('newSession', common.mustNotCall());
  await sendHelloThenClose(sessionServer, hello);
})().then(common.mustCall());
