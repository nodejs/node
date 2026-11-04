'use strict';
const common = require('../common');
if (!common.hasCrypto)
  common.skip('missing crypto');

const assert = require('assert');
const { once } = require('events');
const tls = require('tls');
const fixtures = require('../common/fixtures');

async function connect(options, useSetter = false, captureSession = false) {
  const { session, ...connectOptions } = options;
  const socket = tls.connect(useSetter ? connectOptions : options);
  const closed = new Promise((resolve) => {
    socket.once('close', common.mustCall(resolve));
  });
  if (useSetter)
    socket.setSession(session);
  const sessionPromise = captureSession ? once(socket, 'session') : undefined;
  socket.resume();

  try {
    await once(socket, 'secureConnect');
    const result = {
      reused: socket.isSessionReused(),
      authorized: socket.authorized,
      session: captureSession ? (await sessionPromise)[0] : undefined,
    };
    await closed;
    return result;
  } finally {
    socket.destroy();
  }
}

function offsetView(session, View) {
  // Keep unrelated bytes on both sides to check that only the view is used.
  const buffer = new ArrayBuffer(session.length + 16);
  new Uint8Array(buffer).fill(0xff);
  new Uint8Array(buffer, 8, session.length).set(session);
  return new View(buffer, 8, session.length);
}

async function testVersion(version) {
  const server = tls.createServer({
    key: fixtures.readKey('agent1-key.pem'),
    cert: fixtures.readKey('agent1-cert.pem'),
    minVersion: version,
    maxVersion: version,
  }, (socket) => socket.end('ok'));
  server.listen(0);
  await once(server, 'listening');

  try {
    const options = {
      port: server.address().port,
      host: '127.0.0.1',
      servername: 'agent1',
      ca: fixtures.readKey('ca1-cert.pem'),
      minVersion: version,
      maxVersion: version,
    };
    const first = await connect(options, false, true);
    assert.strictEqual(first.authorized, true);
    assert.strictEqual(first.reused, false);

    const tagged = first.session;
    const prefix = Buffer.from('\0nodejs:tls:session:1\0');
    assert.deepStrictEqual(tagged.subarray(0, prefix.length), prefix);
    const raw = tagged.subarray(
      prefix.length + 2 + tagged.readUInt16BE(prefix.length));

    const conversions = [
      (session) => session,
      (session) => session.toString('latin1'),
      (session) => new Uint8Array(session),
      (session) => new Uint8ClampedArray(session),
      (session) => new Int8Array(session),
      (session) => offsetView(session, Uint8Array),
      (session) => offsetView(session, DataView),
    ];

    for (const convert of conversions) {
      for (const useSetter of [false, true]) {
        // Untagged sessions must cause a full handshake on verified clients.
        const fresh = await connect({
          ...options,
          session: convert(raw),
        }, useSetter);
        assert.strictEqual(fresh.authorized, true);
        assert.strictEqual(fresh.reused, false);

        // A tagged session still resumes for its authenticated host.
        const resumed = await connect({
          ...options,
          session: convert(tagged),
        }, useSetter);
        assert.strictEqual(resumed.authorized, true);
        assert.strictEqual(resumed.reused, true);

        for (const session of [raw, tagged]) {
          await assert.rejects(connect({
            ...options,
            servername: 'bank.example',
            session: convert(session),
          }, useSetter), { code: 'ERR_TLS_CERT_ALTNAME_INVALID' });
        }

        // Raw sessions remain usable when verification is explicitly disabled.
        const unverified = await connect({
          ...options,
          session: convert(raw),
          rejectUnauthorized: false,
        }, useSetter);
        assert.strictEqual(unverified.reused, true);
      }
    }
  } finally {
    server.close();
    await once(server, 'close');
  }
}

(async () => {
  await testVersion('TLSv1.2');
  await testVersion('TLSv1.3');
})().then(common.mustCall());
