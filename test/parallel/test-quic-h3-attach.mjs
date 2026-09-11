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
  sni: { '*': { keys: [key], certs: [cert] } },
};
const clientOpts = {
  alpn: 'h3',
  servername: 'localhost',
  verifyPeer: 'manual',
};
const enc = new TextEncoder();
const dec = new TextDecoder();

// Only a QuicSession can carry an HTTP/3 session.
assert.throws(() => new Http3Session({}), { code: 'ERR_INVALID_ARG_TYPE' });

// Both peers attached after the session already exists, and the attach
// itself validated.
{
  const endpoint = await listen(mustCall((quicSession) => {
    const session = new Http3Session(quicSession);
    assert.strictEqual(session.quicSession, quicSession);

    // Can only attach once:
    assert.throws(() => new Http3Session(quicSession),
                  { code: 'ERR_INVALID_STATE' });
  }), serverOpts);

  const quicClient = await connect(endpoint.address, clientOpts);

  // Options are validated before anything is recorded, so the session is
  // still attachable after these failures:
  assert.throws(() => new Http3Session(quicClient, null),
                { code: 'ERR_INVALID_ARG_TYPE' });
  assert.throws(() => new Http3Session(quicClient, { ongoaway: 5 }),
                { code: 'ERR_INVALID_ARG_TYPE' });

  const client = new Http3Session(quicClient);
  await client.opened;

  assert.strictEqual(client.alpnProtocol, 'h3');
  assert.strictEqual(client.servername, 'localhost');
  assert.strictEqual(typeof client.stats.createdAt, 'bigint');
  assert.strictEqual(typeof client.ephemeralKeyInfo, 'object');
  await client.close();
  await endpoint.close();
}

const tooLate = {
  code: 'ERR_INVALID_STATE',
  message: /already has an application/,
};

// Server: an attach deferred past the session callback is rejected.
{
  const done = Promise.withResolvers();
  const endpoint = await listen(mustCall((quicSession) => {
    setImmediate(mustCall(() => {
      assert.throws(() => new Http3Session(quicSession), tooLate);
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
    new Http3Session(quicSession);
  }), serverOpts);
  const client = await connect(endpoint.address, clientOpts);
  const info = await client.opened;
  assert.strictEqual(info.protocol, 'h3');
  assert.strictEqual(client.alpnProtocol, 'h3');
  // Further already-settled awaits are still the same checkpoint.
  await null;
  const http3 = new Http3Session(client);
  assert.strictEqual(http3.alpnProtocol, 'h3');
  await http3.close();
  await endpoint.close();
}

// Client: yielding to the event loop closes the window, and it is rejected
// twice over - a failed attach must not poison the session into reporting
// some other reason.
{
  const endpoint = await listen(mustCall((quicSession) => {
    new Http3Session(quicSession);
  }), serverOpts);
  const client = await connect(endpoint.address, clientOpts);
  await client.opened;
  await new Promise(setImmediate);
  assert.throws(() => new Http3Session(client), tooLate);
  assert.throws(() => new Http3Session(client), tooLate);

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
  assert.throws(() => new Http3Session(client), tooLate);
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
  assert.throws(() => new Http3Session(client), tooLate);
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
    new Http3Session(quicSession);
  }), serverOpts);
  const client = new Http3Session(
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
    assert.throws(() => new Http3Session(quicSession, { settings }), {
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
  assert.throws(() => new Http3Session(client, { settings }), tooLate);
  await (await raw).closed;
  await client.close();
  await endpoint.close();
}
{
  // Client: the getter attaches another Http3Session. That inner attach is
  // the one that sticks; the outer one finds the session already claimed.
  const endpoint = await listen(mustCall((quicSession) => {
    new Http3Session(quicSession);
  }), serverOpts);

  const client = await connect(endpoint.address, clientOpts);
  let inner;
  const settings = {
    get maxHeaderPairs() { inner = new Http3Session(client); return 10n; },
  };
  assert.throws(() => new Http3Session(client, { settings }), {
    code: 'ERR_INVALID_STATE',
    message: /already has an application/,
  });
  assert.ok(inner instanceof Http3Session);
  assert.throws(() => new Http3Session(client), {
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
    const server = new Http3Session(quicSession);
    assert.throws(() => server.createBidirectionalStream(), {
      code: 'ERR_INVALID_STATE',
      message: /Server sessions cannot open HTTP\/3 request streams/,
    });
    refused.resolve();
  }), serverOpts);
  const client = new Http3Session(await connect(endpoint.address, clientOpts));
  await refused.promise;
  await client.close();
  await endpoint.close();
}
