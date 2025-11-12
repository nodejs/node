// Flags: --experimental-quic
import { hasQuic, isAIX, isIBMi, isWindows, skip } from '../common/index.mjs';
import { rejects } from 'node:assert';

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

const { default: QuicTestClient } = await import('../common/quic/test-client.mjs');

const client = new QuicTestClient();

// If this completes without throwing, the test passes.
await client.help({ stdio: 'ignore' });

setTimeout(() => {
  client.stop();
}, 100);

// We expect this to fail since there's no server running.
await rejects(client.run('localhost', '12345', undefined, { stdio: 'ignore' }),
              { message: /Process exited with code 1 and signal null/ });
