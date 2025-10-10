// Flags: --experimental-quic
import { hasQuic, isAIX, isIBMi, isWindows, skip } from '../common/index.mjs';

if (!hasQuic) {
  skip('QUIC support is not enabled');
}
if (isAIX) {
  // AIX does not support some of the networking features used in the ngtcp2
  // example server and client.
  skip('QUIC third-party tests are disabled on AIX');
}
if (isIBMi) {
  // IBM i does not support some of the networking features used in the ngtcp2
  // example server and client.
  skip('QUIC third-party tests are disabled on IBM i');
}
if (isWindows) {
  // Windows does not support the [Li/U]nix specific headers and system calls
  // required by the ngtcp2 example server/client.
  skip('QUIC third-party tests are disabled on Windows');
}

const { default: QuicTestServer } = await import('../common/quic/test-server.mjs');
const fixtures = await import('../common/fixtures.mjs');

const server = new QuicTestServer();
const fixturesPath = fixtures.path();

// If this completes without throwing, the test passes.
await server.help({ stdio: 'ignore' });

setTimeout(() => {
  server.stop();
}, 100);

await server.run('localhost', '12345',
                 `${fixturesPath}/keys/agent1-key.pem`,
                 `${fixturesPath}/keys/agent1-cert.pem`,
                 { stdio: 'inherit' });
