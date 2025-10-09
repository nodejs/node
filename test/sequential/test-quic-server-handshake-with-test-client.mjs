// Flags: --experimental-quic
import { hasQuic, isAIX, isWindows, skip } from '../common/index.mjs';
import { partialDeepStrictEqual, ok } from 'node:assert';
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

// Import after the hasQuic check
const { default: QuicTestClient } = await import('../common/quic/test-client.mjs');
const { listen } = await import('node:quic');
const { readKey } = await import('../common/fixtures.mjs');
const { createPrivateKey } = await import('node:crypto');

const keys = createPrivateKey(readKey('agent1-key.pem'));
const certs = readKey('agent1-cert.pem');

const check = {
  // The SNI value
  servername: 'localhost',
  // The selected ALPN protocol
  protocol: 'h3',
  // The negotiated cipher suite
  cipher: 'TLS_AES_128_GCM_SHA256',
  cipherVersion: 'TLSv1.3',
};

const serverOpened = Promise.withResolvers();

const server = await listen(async (session) => {
  // Wrapping in a mustCall is not necessary here since if this
  // function is not called the test will time out and fail.
  const info = await session.opened;
  partialDeepStrictEqual(info, check);
  serverOpened.resolve();
  session.destroy();
}, { keys, certs });

// The server must have an address to connect to after listen resolves.
const address = server.address;
ok(address !== undefined);

const client = new QuicTestClient();
client.run(address.address, address.port, undefined, {
  stdio: 'ignore',
});

await serverOpened.promise;

await setTimeout(100);
client.stop();
await server.close();
