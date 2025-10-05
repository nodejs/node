// Flags: --experimental-quic
import { hasQuic, isAIX, isWindows, skip, getPort } from '../common/index.mjs';

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

const { default: QuicTestClient } = await import('../common/quic/test-client.mjs');

const client = new QuicTestClient();

// If this completes without throwing, the test passes.
await client.help({ stdio: 'ignore' });

setTimeout(() => {
  client.stop();
}, 100);

await client.run('localhost', getPort(), undefined, { stdio: 'ignore' });
