// Flags: --experimental-quic --experimental-stream-iter --no-warnings

// Test: HTTP/3 over a QUIC session with NO kApplication preconfigured.
// Wrapping a raw node:quic session with new Http3Session(session) installs
// and starts the HTTP/3 application itself. Covers a working request/response
// with both peers raw, and the attach guards: only before the session becomes
// active (a server in its delivery frame, a client before its handshake
// completes), and no double-wrap.

import { hasQuic, skip, mustCall } from '../common/index.mjs';
import assert from 'node:assert';
import * as fixtures from '../common/fixtures.mjs';

const { strictEqual, throws } = assert;

if (!hasQuic) {
  skip('QUIC is not enabled');
}

const { listen: quicListen, connect: quicConnect } = await import('node:quic');
const { Http3Session } = await import('node:http3');
const { createPrivateKey } = await import('node:crypto');
const { bytes } = await import('stream/iter');

const key = createPrivateKey(fixtures.readKey('agent1-key.pem'));
const cert = fixtures.readKey('agent1-cert.pem');
const serverOpts = { sni: { '*': { keys: [key], certs: [cert] } }, alpn: 'h3' };
const clientOpts = { servername: 'localhost', verifyPeer: 'manual', alpn: 'h3' };
const reqHeaders = {
  ':method': 'GET', ':path': '/', ':scheme': 'https', ':authority': 'localhost',
};
const body = 'Hello from a no-preconfig H3 server';
const enc = new TextEncoder();
const dec = new TextDecoder();

// Working request/response with BOTH peers raw (no kApplication): the server
// installs + starts at its delivery frame, the client before its handshake.
{
  const serverDone = Promise.withResolvers();
  const endpoint = await quicListen(mustCall((quicSession) => {
    const session = new Http3Session(quicSession);
    // The session is now owned; a second wrap of it is rejected.
    throws(() => new Http3Session(quicSession), { code: 'ERR_INVALID_STATE' });
    session.onstream = mustCall(async (stream) => {
      stream.onheaders = mustCall((headers) => {
        strictEqual(headers[':method'], 'GET');
        stream.sendHeaders({ ':status': '200' });
        stream.writer.writeSync(enc.encode(body));
        stream.writer.endSync();
      });
      await stream.closed;
      session.close();
      serverDone.resolve();
    });
  }), serverOpts);

  const client = new Http3Session(await quicConnect(endpoint.address, clientOpts));
  const gotHeaders = Promise.withResolvers();
  const stream = await client.request(reqHeaders, {
    onheaders: mustCall((headers) => {
      strictEqual(headers[':status'], '200');
      gotHeaders.resolve();
    }),
  });
  await gotHeaders.promise;
  strictEqual(dec.decode(await bytes(stream)), body);
  await Promise.all([stream.closed, serverDone.promise]);
  await client.close();
  await endpoint.close();
}

// An application may only be attached before the session becomes active.
const tooLate = { code: 'ERR_INVALID_STATE', message: /before it becomes active/ };

// Server: a wrap deferred past the delivery frame is rejected.
{
  const done = Promise.withResolvers();
  const endpoint = await quicListen(mustCall((quicSession) => {
    setImmediate(mustCall(() => {
      throws(() => new Http3Session(quicSession), tooLate);
      done.resolve();
    }));
  }), serverOpts);
  const client = await quicConnect(endpoint.address, clientOpts);
  await client.opened;
  await done.promise;
  await client.close();
  await endpoint.close();
}

// Client: a wrap after the handshake completes is rejected. A rejected wrap
// must not poison the session - a retry reports the same reason, not a
// spurious 'already has an application attached'.
{
  const wrapped = Promise.withResolvers();
  const endpoint = await quicListen(mustCall((quicSession) => {
    new Http3Session(quicSession);
    wrapped.resolve();
  }), serverOpts);
  const client = await quicConnect(endpoint.address, clientOpts);
  await client.opened;
  await wrapped.promise;
  throws(() => new Http3Session(client), tooLate);
  throws(() => new Http3Session(client), tooLate);
  await client.close();
  await endpoint.close();
}
