// Flags: --experimental-quic --experimental-stream-iter --no-warnings

// Test: attaching HTTP/3 to a node:quic session - when it is allowed, what
// it validates, and how it behaves when the window has closed.

import { hasQuic, skip, mustCall } from '../common/index.mjs';
import assert from 'node:assert';
import * as fixtures from '../common/fixtures.mjs';

if (!hasQuic) {
  skip('QUIC is not enabled');
}

const { listen, connect, Http3Session } = await import('node:quic');
const { createPrivateKey } = await import('node:crypto');
const { bytes } = await import('stream/iter');

const key = createPrivateKey(fixtures.readKey('agent1-key.pem'));
const cert = fixtures.readKey('agent1-cert.pem');
const serverOpts = {
  alpn: ['h3'],
  autoWrap: false,
  sni: { '*': { keys: [key], certs: [cert] } },
};
const clientOpts = {
  alpn: 'h3',
  autoWrap: false,
  servername: 'localhost',
  verifyPeer: 'manual',
};
const enc = new TextEncoder();
const dec = new TextDecoder();

// Only a QuicSession can carry an HTTP/3 session, attached with from():
assert.throws(() => Http3Session.from({}), { code: 'ERR_INVALID_ARG_TYPE' });
assert.throws(() => new Http3Session(), { code: 'ERR_ILLEGAL_CONSTRUCTOR' });

// Both peers attached after the session already exists, and the attach
// itself validated.
{
  const endpoint = await listen(mustCall((quicSession) => {
    const session = Http3Session.from(quicSession);
    assert.strictEqual(session.quicSession, quicSession);

    // Can only attach once:
    assert.throws(() => Http3Session.from(quicSession),
                  { code: 'ERR_INVALID_STATE' });

    // Incoming streams are now reported through the Http3Session only:
    assert.throws(() => { quicSession.onstream = () => {}; }, {
      code: 'ERR_INVALID_STATE',
      message: /cannot be set on a session/,
    });
    // The onerror callback stays transport-level, so both sides keep their own:
    quicSession.onerror = () => {};
    session.onerror = () => {};
    assert.notStrictEqual(quicSession.onerror, session.onerror);
  }), serverOpts);

  const quicClient = await connect(endpoint.address, clientOpts);

  // Options are validated before anything is recorded, so the session is
  // still attachable after these failures:
  assert.throws(() => Http3Session.from(quicClient, null),
                { code: 'ERR_INVALID_ARG_TYPE' });
  assert.throws(() => Http3Session.from(quicClient, { ongoaway: 5 }),
                { code: 'ERR_INVALID_ARG_TYPE' });

  const client = Http3Session.from(quicClient);
  await client.opened;

  // Connection details, TLS included, stay on the QUIC session:
  assert.strictEqual(client.quicSession.alpnProtocol, 'h3');
  assert.strictEqual(client.quicSession.servername, 'localhost');
  assert.strictEqual('peerCertificate' in client, false);
  assert.strictEqual(typeof client.stats.createdAt, 'bigint');
  assert.strictEqual(client.closing, client.quicSession.closing);
  await client.close();
  await endpoint.close();
}

const tooLate = {
  code: 'ERR_INVALID_STATE',
  message: /already has an application/,
};

// Setting onstream claims the session for raw QUIC, so HTTP/3 can't be
// attached afterwards, whether it is set directly or passed as an option.
{
  const endpoint = await listen(mustCall((quicSession) => {
    quicSession.onstream = () => {};
    assert.throws(() => Http3Session.from(quicSession), tooLate);
  }), serverOpts);
  const client = await connect(endpoint.address,
                               { ...clientOpts, onstream: () => {} });
  assert.throws(() => Http3Session.from(client), tooLate);
  await client.opened;
  await client.close();
  await endpoint.close();
}

// Server: an attach deferred past the session callback is rejected.
{
  const done = Promise.withResolvers();
  const endpoint = await listen(mustCall((quicSession) => {
    setImmediate(mustCall(() => {
      assert.throws(() => Http3Session.from(quicSession), tooLate);
      done.resolve();
    }));
  }), serverOpts);
  const client = await connect(endpoint.address, clientOpts);
  await client.opened;
  await done.promise;
  await client.close();
  await endpoint.close();
}

// Client: the window stays open across the microtask checkpoint that resolves
// `opened`, so the negotiated ALPN can be read and acted on before attaching.
{
  const endpoint = await listen(mustCall((quicSession) => {
    Http3Session.from(quicSession);
  }), serverOpts);
  const client = await connect(endpoint.address, clientOpts);
  const info = await client.opened;
  assert.strictEqual(info.protocol, 'h3');
  assert.strictEqual(client.alpnProtocol, 'h3');
  // Further already-settled awaits are still the same checkpoint.
  await null;
  const http3 = Http3Session.from(client);
  assert.strictEqual(http3.quicSession, client);
  await http3.close();
  await endpoint.close();
}

// Client: yielding to the event loop closes the window, and it is rejected
// twice over - a failed attach must not poison the session into reporting
// some other reason.
{
  const endpoint = await listen(mustCall((quicSession) => {
    Http3Session.from(quicSession);
  }), serverOpts);
  const client = await connect(endpoint.address, clientOpts);
  await client.opened;
  await new Promise(setImmediate);
  assert.throws(() => Http3Session.from(client), tooLate);
  assert.throws(() => Http3Session.from(client), tooLate);

  await client.close();
  await endpoint.close();
}

// Client: an attach once any stream exists is rejected, even pre-handshake.
{
  const serverGot = Promise.withResolvers();
  const endpoint = await listen(mustCall((quicSession) => {
    quicSession.onstream = mustCall(async (stream) => {
      assert.strictEqual(dec.decode(await bytes(stream)), 'x');
      quicSession.close();
      serverGot.resolve();
    });
  }), serverOpts);
  const client = await connect(endpoint.address, clientOpts);
  const raw = await client.createUnidirectionalStream({ body: enc.encode('x') });
  assert.throws(() => Http3Session.from(client), tooLate);
  await client.opened;
  await serverGot.promise;
  await raw.closed;
  await client.close();
  await endpoint.close();
}

// Client: sending a datagram attaches the application.
{
  const dgramOpts = { transportParams: { maxDatagramFrameSize: 100 } };
  const endpoint = await listen(mustCall((quicSession) => {
    quicSession.closed.catch(() => {});
  }), { ...serverOpts, ...dgramOpts });
  const client = await connect(endpoint.address, { ...clientOpts, ...dgramOpts });
  await client.opened;
  await client.sendDatagram(enc.encode('x'));
  assert.throws(() => Http3Session.from(client), tooLate);
  await client.close();
  await endpoint.close();
}

// Settings are validated before anything is recorded, so a rejected value
// names the property at fault and leaves the session still attachable.
{
  const endpoint = await listen(mustCall((quicSession) => {
    Http3Session.from(quicSession);
  }), serverOpts);
  const client = await connect(endpoint.address, clientOpts);
  for (const settings of [42, true, 'nope', null]) {
    assert.throws(() => Http3Session.from(client, { settings }),
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
    assert.throws(() => Http3Session.from(client, { settings }), (err) => {
      assert.strictEqual(err.code, expected.code);
      assert.match(err.message, /options\.settings\./);
      return true;
    });
  }
  // Numbers are accepted for bigint settings:
  const http3 = Http3Session.from(client, { settings: { maxHeaderPairs: 12 } });
  assert.strictEqual(http3.settings.maxHeaderPairs, 12n);
  await http3.opened;
  await client.close();
  await endpoint.close();
}

// A client that opens no stream gets its application installed when the
// handshake completes, which may be well after the attach. The settings it
// asked for should survive the gap, with anything else left as default.
{
  const settings = {
    maxHeaderPairs: 33n,
    qpackBlockedStreams: 77n,
    enableConnectProtocol: false,
  };
  const endpoint = await listen(mustCall((quicSession) => {
    Http3Session.from(quicSession);
  }), serverOpts);
  const client = Http3Session.from(
    await connect(endpoint.address, clientOpts), { settings });

  await client.opened;
  const applied = client.settings;
  assert.strictEqual(applied.maxHeaderPairs, 33n);
  assert.strictEqual(applied.qpackBlockedStreams, 77n);
  assert.strictEqual(applied.enableConnectProtocol, false);
  assert.strictEqual(applied.qpackMaxDtableCapacity, 4096n);
  await client.close();
  await endpoint.close();
}

// Parsing the settings runs their property getters, i.e. arbitrary JS, part
// way through the attach. We should safely handle even the weirdest things
// you could do as part of that:
{
  // Server: the getter destroys the session.
  const done = Promise.withResolvers();
  const endpoint = await listen(mustCall((quicSession) => {
    const settings = {
      get maxHeaderPairs() { quicSession.destroy(); return 10n; },
    };
    assert.throws(() => Http3Session.from(quicSession, { settings }), {
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
  // Client: the getter creates a stream.
  const endpoint = await listen(mustCall((quicSession) => {
    quicSession.onstream = mustCall(async (stream) => {
      assert.strictEqual(dec.decode(await bytes(stream)), 'x');
      quicSession.close();
    });
  }), serverOpts);

  const client = await connect(endpoint.address, clientOpts);
  let raw;
  const settings = {
    get maxHeaderPairs() {
      raw = client.createUnidirectionalStream({ body: enc.encode('x') });
      return 10n;
    },
  };
  assert.throws(() => Http3Session.from(client, { settings }), tooLate);
  await (await raw).closed;
  await client.close();
  await endpoint.close();
}
{
  // Client: the getter attaches another Http3Session. That inner attach is
  // the one that sticks; the outer one finds the session already claimed.
  const endpoint = await listen(mustCall((quicSession) => {
    Http3Session.from(quicSession);
  }), serverOpts);

  const client = await connect(endpoint.address, clientOpts);
  let inner;
  const settings = {
    get maxHeaderPairs() { inner = Http3Session.from(client); return 10n; },
  };
  assert.throws(() => Http3Session.from(client, { settings }), {
    code: 'ERR_INVALID_STATE',
    message: /already has an application/,
  });
  assert.ok(inner instanceof Http3Session);
  assert.throws(() => Http3Session.from(client), {
    code: 'ERR_INVALID_STATE',
    message: /already has an application/,
  });
  await client.opened;
  await client.close();
  await endpoint.close();
}

// HTTP/3 has no server-initiated request streams, so a server session must
// refuse to open one however it is asked.
{
  const refused = Promise.withResolvers();
  const endpoint = await listen(mustCall((quicSession) => {
    const server = Http3Session.from(quicSession);
    refused.resolve(assert.rejects(server.createBidirectionalStream(), {
      code: 'ERR_INVALID_STATE',
      message: /Server sessions cannot open HTTP\/3 request streams/,
    }));
  }), serverOpts);
  const client = Http3Session.from(await connect(endpoint.address, clientOpts));
  await refused.promise;
  await client.opened;
  await client.close();
  await endpoint.close();
}
