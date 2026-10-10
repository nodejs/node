// Flags: --experimental-quic --no-warnings

// Test: starting HTTP/3 on a node:quic connection - when it is allowed, what
// it validates, and how it behaves when the window has closed.

import { hasQuic, skip, mustCall } from '../common/index.mjs';
import assert from 'node:assert';
import * as fixtures from '../common/fixtures.mjs';

if (!hasQuic) {
  skip('QUIC is not enabled');
}

const { listen, connect, Http3Session, QuicSession } = await import('node:quic');
const { createPrivateKey } = await import('node:crypto');

const key = createPrivateKey(fixtures.readKey('agent1-key.pem'));
const cert = fixtures.readKey('agent1-cert.pem');
const serverOpts = {
  alpn: ['h3'],
  autoStart: false,
  sni: { '*': { keys: [key], certs: [cert] } },
};
const clientOpts = {
  alpn: 'h3',
  autoStart: false,
  servername: 'localhost',
  verifyPeer: 'manual',
};

// Only a QuicConnection can carry a session, started with start():
for (const Session of [Http3Session, QuicSession]) {
  assert.throws(() => Session.start({}), { code: 'ERR_INVALID_ARG_TYPE' });
  assert.throws(() => new Session(), { code: 'ERR_ILLEGAL_CONSTRUCTOR' });
}

// Validate manually starting both sides of an HTTP/3 session:
{
  const endpoint = await listen(mustCall((connection) => {
    const session = Http3Session.start(connection);
    assert.strictEqual(session.connection, connection);

    // Can only start once:
    assert.throws(() => Http3Session.start(connection),
                  { code: 'ERR_INVALID_STATE' });
  }), serverOpts);

  const quicClient = await connect(endpoint.address, clientOpts);

  // Options are validated before anything is recorded, so a session can
  // still be started after these failures:
  assert.throws(() => Http3Session.start(quicClient, null),
                { code: 'ERR_INVALID_ARG_TYPE' });
  assert.throws(() => Http3Session.start(quicClient, { ongoaway: 5 }),
                { code: 'ERR_INVALID_ARG_TYPE' });

  const client = Http3Session.start(quicClient);
  await client.opened;

  // Connection details, TLS included, stay on the QUIC connection:
  assert.strictEqual(client.connection.alpnProtocol, 'h3');
  assert.strictEqual(client.connection.servername, 'localhost');
  assert.strictEqual('peerCertificate' in client, false);
  assert.strictEqual(typeof client.stats.createdAt, 'bigint');
  assert.strictEqual(client.closing, client.connection.closing);
  await client.close();
  await endpoint.close();
}

const alreadyStarted = {
  code: 'ERR_INVALID_STATE',
  message: /already has a session started/,
};
const connectionRefused = { code: 'ERR_QUIC_TRANSPORT_ERROR', message: /CONNECTION_REFUSED/ };
const destroyed = { code: 'ERR_INVALID_STATE', message: /destroyed/ };

// A raw QuicSession started first rules out HTTP/3.
{
  const endpoint = await listen(mustCall((connection) => {
    QuicSession.start(connection);
    assert.throws(() => Http3Session.start(connection), alreadyStarted);
  }), serverOpts);
  const connection = await connect(endpoint.address, clientOpts);
  const client = QuicSession.start(connection);
  assert.throws(() => Http3Session.start(connection), alreadyStarted);
  await client.opened;
  await client.close();
  await endpoint.close();
}

// Server: a connection with no session started by the end of the session
// callback is closed with an error, so a deferred start is too late.
{
  const done = Promise.withResolvers();
  const endpoint = await listen(mustCall((connection) => {
    setImmediate(mustCall(() => {
      assert.throws(() => Http3Session.start(connection), destroyed);
      done.resolve();
    }));
  }), serverOpts);
  const client = await connect(endpoint.address, clientOpts);
  await assert.rejects(client.opened, connectionRefused);
  await done.promise;
  await endpoint.close();
}

// Client: the window stays open during the tick that resolves `opened`, so
// the negotiated ALPN can be read and acted on before starting.
{
  const endpoint = await listen(mustCall((connection) => {
    Http3Session.start(connection);
  }), serverOpts);
  const client = await connect(endpoint.address, clientOpts);
  const info = await client.opened;
  assert.strictEqual(info.protocol, 'h3');
  assert.strictEqual(client.alpnProtocol, 'h3');
  // Further already-settled awaits are still the same checkpoint.
  await null;
  const http3 = Http3Session.start(client);
  assert.strictEqual(http3.connection, client);
  await http3.close();
  await endpoint.close();
}

// Client: yielding to the event loop closes the window, closing the
// connection with an error as no session was started on it.
{
  const endpoint = await listen(mustCall((connection) => {
    Http3Session.start(connection).closed.catch(() => {});
  }), serverOpts);
  const client = await connect(endpoint.address, clientOpts);
  await client.opened;
  await new Promise(setImmediate);
  await assert.rejects(client.closed, {
    code: 'ERR_QUIC_TRANSPORT_ERROR',
    message: /INTERNAL_ERROR/,
  });
  assert.throws(() => Http3Session.start(client), destroyed);
  await endpoint.close();
}

// Settings are validated before anything is recorded, so a rejected value
// names the property at fault and leaves the connection still startable.
{
  const endpoint = await listen(mustCall((connection) => {
    Http3Session.start(connection);
  }), serverOpts);
  const client = await connect(endpoint.address, clientOpts);
  for (const settings of [42, true, 'nope', null]) {
    assert.throws(() => Http3Session.start(client, { settings }),
                  { code: 'ERR_INVALID_ARG_TYPE', message: /options\.settings/ });
  }
  const badType = { code: 'ERR_INVALID_ARG_TYPE' };
  const badRange = { code: 'ERR_OUT_OF_RANGE' };
  for (const [settings, expected] of [
    [{ maxHeaderPairs: 'lots' }, badType],
    [{ maxHeaderPairs: 1.5 }, badRange],
    [{ qpackBlockedStreams: 1n << 65n }, badRange],
    [{ enableDatagrams: 1 }, badType],
  ]) {
    assert.throws(() => Http3Session.start(client, { settings }), (err) => {
      assert.strictEqual(err.code, expected.code);
      assert.match(err.message, /options\.settings\./);
      return true;
    });
  }
  // Numbers are accepted for bigint settings, which stay in effect through the
  // handshake, with anything else left as default:
  const http3 = Http3Session.start(client, { settings: { maxHeaderPairs: 12 } });
  assert.strictEqual(http3.settings.maxHeaderPairs, 12n);
  await http3.opened;
  assert.strictEqual(http3.settings.maxHeaderPairs, 12n);
  assert.strictEqual(http3.settings.qpackMaxDtableCapacity, 4096n);
  await http3.close();
  await endpoint.close();
}

// Parsing the settings runs their property getters, i.e. arbitrary JS, part
// way through the start. We should safely handle even the weirdest things
// you could do as part of that:
{
  // Server: the getter destroys the connection.
  const done = Promise.withResolvers();
  const endpoint = await listen(mustCall((connection) => {
    const settings = {
      get maxHeaderPairs() { connection.destroy(); return 10n; },
    };
    assert.throws(() => Http3Session.start(connection, { settings }), {
      code: 'ERR_INVALID_STATE',
      message: /destroyed/,
    });
    done.resolve();
  }), serverOpts);
  const client = await connect(endpoint.address, clientOpts);
  await done.promise;
  client.destroy();
  await endpoint.close();
}
{
  // Client: the getter starts another Http3Session. That inner start is the
  // one that sticks; the outer one finds the connection already taken.
  const endpoint = await listen(mustCall((connection) => {
    Http3Session.start(connection);
  }), serverOpts);

  const client = await connect(endpoint.address, clientOpts);
  let inner;
  const settings = {
    get maxHeaderPairs() { inner = Http3Session.start(client); return 10n; },
  };
  assert.throws(() => Http3Session.start(client, { settings }), alreadyStarted);
  assert.ok(inner instanceof Http3Session);
  assert.throws(() => Http3Session.start(client), alreadyStarted);
  await inner.opened;
  await inner.close();
  await endpoint.close();
}

// HTTP/3 has no server-initiated request streams, so a server session must
// refuse to open one however it is asked.
{
  const refused = Promise.withResolvers();
  const endpoint = await listen(mustCall((connection) => {
    const server = Http3Session.start(connection);
    refused.resolve(assert.rejects(server.createBidirectionalStream(), {
      code: 'ERR_INVALID_STATE',
      message: /Server sessions cannot open HTTP\/3 request streams/,
    }));
  }), serverOpts);
  const client = Http3Session.start(await connect(endpoint.address, clientOpts));
  await refused.promise;
  await client.opened;
  await client.close();
  await endpoint.close();
}
