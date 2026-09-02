// Flags: --experimental-quic --experimental-stream-iter --no-warnings

// Test: the server stops the TLS handshake at the ClientHello, so a session
// reaches JavaScript before anything that belongs to it - even a 0-RTT
// request the client put in its very first flight. The stop also has to
// survive the ClientHello being seen more than once.

import { hasQuic, skip, mustCall } from '../common/index.mjs';
import assert from 'node:assert';

if (!hasQuic) {
  skip('QUIC is not enabled');
}

const { listen, connect } = await import('../common/quic.mjs');
const { bytes } = await import('stream/iter');

const encoder = new TextEncoder();
const decoder = new TextDecoder();

// Send two requests, the 2nd via 0RTT, make sure the stream event always
// fires _after_ the session event has completed (even though 0RTT is
// delivered already in the first flight - delivery must be deferred).
{
  let ticket;
  let token;
  const gotTicket = Promise.withResolvers();
  const gotToken = Promise.withResolvers();

  const endpoint = await listen(mustCall((ss) => {
    // No streams initially, stream must arrive in the onstream event, for
    // both the normal and the 0RTT sessions:
    assert.strictEqual(ss.stats.bidiInStreamCount, 0n);
    ss.onstream = mustCall(async (stream) => {
      await stream.closed;
      ss.close();
    });
  }, 2), {
    alpn: ['h3'],
    onheaders: mustCall(function() {
      this.sendHeaders({ ':status': '200' });
      this.writer.writeSync(encoder.encode('hello'));
      this.writer.endSync();
    }, 2),
  });

  const request = {
    ':method': 'GET',
    ':path': '/',
    ':scheme': 'https',
    ':authority': 'localhost',
  };

  // Open a 1st session, send a request, get session ticket & token:
  const cs1 = await connect(endpoint.address, {
    servername: 'localhost',
    alpn: 'h3',
    onsessionticket: mustCall((t) => { ticket = t; gotTicket.resolve(); }, 2),
    onnewtoken: mustCall((t) => { token = t; gotToken.resolve(); }),
  });
  await cs1.opened;
  await Promise.all([gotTicket.promise, gotToken.promise]);
  const s1 = await cs1.createBidirectionalStream({
    headers: request,
    onheaders: mustCall(),
  });
  await bytes(s1);
  await Promise.all([s1.closed, cs1.closed]);

  // Open 2nd session, reusing the ticket & token:
  const cs2 = await connect(endpoint.address, {
    servername: 'localhost',
    alpn: 'h3',
    sessionTicket: ticket,
    token,
  });

  // Send a 0RTT request immediately, before the handshake completes:
  const s2 = await cs2.createBidirectionalStream({
    headers: request,
    onheaders: mustCall(),
  });

  const info = await cs2.opened;
  assert.strictEqual(info.earlyDataAccepted, true);
  assert.strictEqual(decoder.decode(await bytes(s2)), 'hello');
  await Promise.all([s2.closed, cs2.closed]);
  await endpoint.close();
}

// When a handshake does HelloRetryRequest (HRR) and runs the hello flow twice,
// we must preserve the ALPN and server name selection from the first hello. To
// trigger this, we send a hello with an offer for X25519 & P-521, but only a
// key share for X25519 (the *) so an automatic HRR is required.
{
  const serverDone = Promise.withResolvers();
  const endpoint = await listen(mustCall((ss) => {
    ss.opened.then(mustCall((info) => {
      assert.strictEqual(info.servername, 'example.test');
      assert.strictEqual(info.protocol, 'quic-test');
      serverDone.resolve();
    }));
  }), { groups: 'P-521' });

  const cs = await connect(endpoint.address, {
    servername: 'example.test',
    groups: '*X25519:P-521',
  });

  const info = await cs.opened;
  assert.strictEqual(info.protocol, 'quic-test');
  // Validate the HRR happened: we fell back to 2nd group
  assert.strictEqual(cs.ephemeralKeyInfo.name, 'secp521r1');

  await serverDone.promise;
  cs.close();
  await endpoint.close();
}
