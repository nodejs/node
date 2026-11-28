// Flags: --experimental-dtls --no-warnings

// Test: a server with DTLS 1.3 enabled validates each peer's address before
// announcing a session for it.
//
// DTLSv1_listen() only speaks HelloVerifyRequest and caps the handshake at
// DTLS 1.2, so once DTLS 1.3 is enabled the server keeps a pending SSL per
// peer instead and lets OpenSSL send whichever of HelloVerifyRequest or
// HelloRetryRequest the negotiated version calls for. These cases watch the
// wire through a proxy to check that it does, that a session is only
// announced once the cookie has come back from the address it was sent to,
// and that losing the cookie flight does not strand the handshake.

import { hasCrypto, mustCall, mustNotCall, skip } from '../common/index.mjs';
import * as fixtures from '../common/fixtures.mjs';
import assert from 'node:assert';
import { once } from 'node:events';

if (!hasCrypto) {
  skip('missing crypto');
}

if (!process.features.dtls) {
  skip('DTLS is not enabled');
}

const dgram = await import('node:dgram');
const { connect, listen } = await import('node:dtls');

const HOST = '127.0.0.1';
const cert = fixtures.readKey('agent1-cert.pem').toString();
const key = fixtures.readKey('agent1-key.pem').toString();

const V12 = { minVersion: 'DTLSv1.2', maxVersion: 'DTLSv1.2' };
const V13 = { minVersion: 'DTLSv1.3', maxVersion: 'DTLSv1.3' };
const BOTH = { minVersion: 'DTLSv1.2', maxVersion: 'DTLSv1.3' };

// SHA-256("HelloRetryRequest"), the ServerHello.random that marks a
// HelloRetryRequest (RFC 8446 section 4.1.3).
const HRR_RANDOM = Buffer.from(
  'cf21ad74e59a6111be1d8c021e65b891c2a211167abb8c5e079e09e2c8a8339c', 'hex');

// The first handshake message in a plaintext DTLS record: a 13 byte record
// header, then msg_type, and for a ServerHello its random 14 bytes later.
function classify(datagram) {
  if (datagram.length < 14 || datagram[0] !== 22) return 'other';
  switch (datagram[13]) {
    case 1: return 'ClientHello';
    case 3: return 'HelloVerifyRequest';
    case 2:
      return datagram.length >= 59 &&
        datagram.subarray(27, 59).equals(HRR_RANDOM) ?
        'HelloRetryRequest' : 'ServerHello';
    default: return 'other';
  }
}

// A one-client UDP proxy that records, and can drop, what passes through.
async function createProxy(serverPort, { dropFromServer = () => false } = {}) {
  const front = dgram.createSocket('udp4');
  const back = dgram.createSocket('udp4');
  const fromClient = [];
  const fromServer = [];
  let client;
  front.on('message', (msg, rinfo) => {
    client = rinfo;
    fromClient.push(msg);
    back.send(msg, serverPort, HOST);
  });
  back.on('message', (msg) => {
    fromServer.push(msg);
    if (dropFromServer(msg, fromServer.length)) return;
    if (client) front.send(msg, client.port, client.address);
  });
  front.bind(0, HOST);
  back.bind(0, HOST);
  await Promise.all([once(front, 'listening'), once(back, 'listening')]);
  return {
    port: front.address().port,
    fromClient,
    fromServer,
    close() { front.close(); back.close(); },
  };
}

// DTLS 1.3 validates with a HelloRetryRequest, and the ClientHello it answers
// does not fit in one datagram at the default MTU -- the post-quantum key
// share alone is over a kilobyte -- so it arrives in fragments.
{
  const endpoint = listen(mustCall(), { cert, key, host: HOST, port: 0, ...BOTH });
  const proxy = await createProxy(endpoint.address.port);

  const client = connect(HOST, proxy.port, { rejectUnauthorized: false, ...V13 });
  const { protocol } = await client.opened;
  assert.strictEqual(protocol, 'DTLSv1.3');

  assert.strictEqual(classify(proxy.fromServer[0]), 'HelloRetryRequest');
  const hellos = proxy.fromClient.filter((d) => classify(d) === 'ClientHello');
  // At least two datagrams for each of the two ClientHellos.
  assert.ok(hellos.length >= 4, `${hellos.length} ClientHello datagrams`);

  await client.close();
  await endpoint.close();
  proxy.close();
}

// The same server answers a DTLS 1.2 client with a HelloVerifyRequest.
{
  const endpoint = listen(mustCall(), { cert, key, host: HOST, port: 0, ...BOTH });
  const proxy = await createProxy(endpoint.address.port);

  const client = connect(HOST, proxy.port, { rejectUnauthorized: false, ...V12 });
  assert.strictEqual((await client.opened).protocol, 'DTLSv1.2');
  assert.strictEqual(classify(proxy.fromServer[0]), 'HelloVerifyRequest');

  await client.close();
  await endpoint.close();
  proxy.close();
}

// A lost HelloRetryRequest is recovered by the client retransmitting its
// first ClientHello. The server keeps no retransmit timer of its own for a
// peer it has not validated; it answers the retransmission.
{
  const endpoint = listen(mustCall(), { cert, key, host: HOST, port: 0, ...BOTH });
  const proxy = await createProxy(endpoint.address.port, {
    dropFromServer: (msg, n) => n === 1,
  });

  const client = connect(HOST, proxy.port, { rejectUnauthorized: false, ...V13 });
  assert.strictEqual((await client.opened).protocol, 'DTLSv1.3');
  const retries = proxy.fromServer.filter(
    (d) => classify(d) === 'HelloRetryRequest');
  assert.ok(retries.length >= 2, `${retries.length} HelloRetryRequests`);

  await client.close();
  await endpoint.close();
  proxy.close();
}

// Replaying a client's datagrams from a different address gets the replayer
// no session. The first ClientHello only earns a HelloRetryRequest, which is
// smaller than what was sent, and the second carries a cookie bound to the
// original address, which does not verify from here and is answered with the
// illegal_parameter alert RFC 9147 section 5.1 requires. That alert is sent
// after the server has decided against the peer, so once it arrives nothing
// more can be announced.
{
  const announced = [];
  const endpoint = listen((session) => announced.push(session),
                          { cert, key, host: HOST, port: 0, ...BOTH });
  const proxy = await createProxy(endpoint.address.port);

  const client = connect(HOST, proxy.port, { rejectUnauthorized: false, ...V13 });
  await client.opened;
  assert.strictEqual(announced.length, 1);
  await client.close();

  const hellos = proxy.fromClient.filter((d) => classify(d) === 'ClientHello');
  const replayer = dgram.createSocket('udp4');
  const replies = [];
  const answered = Promise.withResolvers();
  replayer.on('message', (msg) => {
    replies.push(msg);
    if (replies.length === 2) answered.resolve();
  });
  replayer.bind(0, HOST);
  await once(replayer, 'listening');

  let sent = 0;
  for (const datagram of hellos) {
    sent += datagram.length;
    replayer.send(datagram, endpoint.address.port, HOST);
  }
  await answered.promise;

  assert.strictEqual(classify(replies[0]), 'HelloRetryRequest');
  // A plaintext alert record: content type 21, level fatal(2),
  // illegal_parameter(47).
  assert.deepStrictEqual([replies[1][0], replies[1][13], replies[1][14]],
                         [21, 2, 47]);
  assert.strictEqual(announced.length, 1);
  const received = replies.reduce((n, d) => n + d.length, 0);
  assert.ok(received < sent, `${received} bytes back for ${sent} sent`);

  replayer.close();
  await endpoint.close();
  proxy.close();
}

// Garbage that is not a ClientHello is rejected before anything is created
// for it, and gets no reply. The handshake that follows cannot complete
// until the server has read everything queued ahead of it, so by then the
// rejection has been counted.
{
  const endpoint = listen(mustCall(), { cert, key, host: HOST, port: 0, ...BOTH });
  const junk = dgram.createSocket('udp4');
  junk.on('message', mustNotCall());
  junk.bind(0, HOST);
  await once(junk, 'listening');
  const sent = Promise.withResolvers();
  junk.send(Buffer.alloc(64, 0x16), endpoint.address.port, HOST,
            (err) => (err ? sent.reject(err) : sent.resolve()));
  await sent.promise;

  const client = connect(HOST, endpoint.address.port, {
    rejectUnauthorized: false, ...BOTH,
  });
  assert.strictEqual((await client.opened).protocol, 'DTLSv1.3');
  assert.strictEqual(endpoint.stats.serverRejectedCount, 1n);
  await client.close();
  junk.close();
  await endpoint.close();
}

// maxSessions counts a session from the moment its cookie verifies, so a
// second peer is refused while the first is open.
{
  const endpoint = listen(mustCall(), {
    cert, key, host: HOST, port: 0, maxSessions: 1, ...BOTH,
  });
  const first = connect(HOST, endpoint.address.port, {
    rejectUnauthorized: false, ...V13,
  });
  await first.opened;

  const second = connect(HOST, endpoint.address.port, {
    rejectUnauthorized: false, handshakeTimeout: 500, ...V13,
  });
  await assert.rejects(second.opened, /handshake timeout/);

  await first.close();
  await endpoint.close();
}
