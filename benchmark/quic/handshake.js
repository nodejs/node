'use strict';

// Measures the cost of establishing QUIC sessions: how many complete
// handshakes per second a single endpoint can serve, for raw QUIC and for
// HTTP/3. Nothing is sent on the session beyond what the protocol itself
// requires, so this isolates connection setup rather than data transfer.

const common = require('../common.js');
const fixtures = require('../../test/common/fixtures');
const { createPrivateKey } = require('crypto');

const bench = common.createBenchmark(main, {
  // 'raw' negotiates a non-HTTP ALPN and does no application work.
  // 'h3' installs HTTP/3 on every session, so each peer also builds an
  // nghttp3 connection and its control/QPACK streams.
  protocol: ['raw', 'h3'],
  concurrency: [1, 10],
  n: [1000],
}, { flags: ['--experimental-quic', '--no-warnings'] });

async function main({ protocol, concurrency, n }) {
  const { listen, connect, Http3Session } = require('node:quic');

  const key = createPrivateKey(fixtures.readKey('agent1-key.pem'));
  const cert = fixtures.readKey('agent1-cert.pem');
  const http3 = protocol === 'h3';
  const alpn = http3 ? 'h3' : 'quic-bench';

  const endpoint = await listen((quicSession) => {
    const session = http3 ? new Http3Session(quicSession) : quicSession;
    // A benchmark peer never reads these; swallow so a torn-down session
    // cannot produce an unhandled rejection.
    session.opened.catch(() => {});
    session.closed.catch(() => {});
  }, {
    sni: { '*': { keys: [key], certs: [cert] } },
    alpn: [alpn],
    // The defaults rate-limit session creation per host, which a benchmark
    // hammering a single address would otherwise trip.
    endpoint: {
      maxConnectionsPerHost: 0xFFFF,
      maxConnectionsTotal: 0xFFFF,
      sessionCreationRate: 1_000_000,
      sessionCreationBurst: 1_000_000,
    },
  });

  const address = endpoint.address;

  async function handshake() {
    const quicSession = await connect(address, {
      servername: 'localhost',
      verifyPeer: 'manual',
      alpn,
    });
    const session = http3 ? new Http3Session(quicSession) : quicSession;
    await session.opened;
    session.close();
    await session.closed.catch(() => {});
  }

  async function run(count) {
    for (let i = 0; i < count; i += concurrency) {
      const batch = Math.min(concurrency, count - i);
      await Promise.all(Array.from({ length: batch }, handshake));
    }
  }

  // Warm up the TLS and QUIC machinery before measuring.
  await run(Math.min(100, n));

  bench.start();
  await run(n);
  bench.end(n);

  await endpoint.close();
}
