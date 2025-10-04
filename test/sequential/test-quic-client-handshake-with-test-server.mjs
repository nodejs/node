// Flags: --experimental-quic
import { hasQuic, isAIX, isWindows, skip, getPort } from '../common/index.mjs';
import { partialDeepStrictEqual } from 'node:assert';
import { setTimeout } from 'node:timers/promises';

if (!hasQuic) {
  skip('QUIC support is not enabled');
}
if (isAIX) {
  // AIX does not support some of the networking features used in the ngtcp2
  // example server and client.
  skip('QUIC third-party tests are disabled on AIX');
}
if (isWindows) {
  // Windows does not support the [Li/U]nix specific headers and system calls
  // required by the ngtcp2 example server/client.
  skip('QUIC third-party tests are disabled on Windows');
}

// Test that our QUIC client can successfully handshake with the ngtcp2
// test server (not our implementation).

const { default: QuicTestServer } = await import('../common/quic/test-server.mjs');
const { connect } = await import('node:quic');
const fixtures = await import('../common/fixtures.mjs');
const kPort = getPort();

const server = new QuicTestServer();
const fixturesPath = fixtures.path();

// If this completes without throwing, the test passes.
await server.help({ stdio: 'ignore' });

server.run('localhost', kPort,
           `${fixturesPath}/keys/agent1-key.pem`,
           `${fixturesPath}/keys/agent1-cert.pem`,
           { stdio: 'ignore' });

const client = await connect({
  host: 'localhost',
  port: kPort
});

const check = {
  servername: 'localhost',
  protocol: 'h3',
  cipher: 'TLS_AES_128_GCM_SHA256',
  cipherVersion: 'TLSv1.3',
};

// If the handshake completes without throwing the opened promise
// will be resolved with the basic handshake info.
const info = await client.opened;

partialDeepStrictEqual(info, check);

client.close();

await setTimeout(100);
server.stop();
