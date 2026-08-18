// Flags: --experimental-dtls --no-warnings

// Test: DTLS interop -- Node.js DTLS server with OpenSSL s_client.
// Verifies that an external DTLS client (OpenSSL CLI) can complete a
// handshake with Node's DTLS server and exchange application data.

import { hasCrypto, skip, mustCall } from '../common/index.mjs';
import { createRequire } from 'module';
import assert from 'node:assert';
import { spawn } from 'node:child_process';
import { setTimeout as sleep } from 'node:timers/promises';
import * as fixtures from '../common/fixtures.mjs';

if (!hasCrypto) {
  skip('missing crypto');
}

if (!process.features.dtls) {
  skip('DTLS is not enabled');
}

const require = createRequire(import.meta.url);
const { opensslCli } = require('../common/crypto');

if (!opensslCli) {
  skip('missing openssl-cli');
}

const { listen } = await import('node:dtls');

const reply = 'I AM THE WALRUS'; // Something recognizable
const serverReceivedData = Promise.withResolvers();

// Start Node.js DTLS server.
const endpoint = listen(mustCall((session) => {
  session.onmessage = mustCall((data) => {
    assert.strictEqual(data.toString().trim(), 'hello from openssl');
    session.send(reply);
    serverReceivedData.resolve();
  });
  session.onhandshake = mustCall();
}), {
  cert: fixtures.readKey('agent1-cert.pem').toString(),
  key: fixtures.readKey('agent1-key.pem').toString(),
  port: 0,
  host: '127.0.0.1',
});

const { port } = endpoint.address;

// Spawn OpenSSL s_client to connect to the Node.js server.
const args = [
  's_client',
  '-dtls',
  '-connect', `127.0.0.1:${port}`,
  '-CAfile', fixtures.path('keys/ca1-cert.pem'),
];

const client = spawn(opensslCli, args, { stdio: 'pipe' });

let stdout = '';
client.stdout.on('data', (data) => { stdout += data; });

let stderr = '';
client.stderr.on('data', (data) => { stderr += data; });

const timeout = setTimeout(() => {
  client.kill();
  endpoint.close();
  assert.fail('Test timed out');
}, 10000);

// Wait for the handshake to start (s_client writes TLS info to stdout),
// then send data.
await new Promise((resolve) => client.stdout.once('data', resolve));
await sleep(500);

client.stdin.write('hello from openssl\n');

// Wait for the server to receive and reply.
await serverReceivedData.promise;
await sleep(500);

// Close stdin so s_client exits.
client.stdin.end();

// Wait for s_client to exit.
const code = await new Promise((resolve) => client.on('close', resolve));
clearTimeout(timeout);

// s_client should exit cleanly.
assert.strictEqual(code, 0,
                   `openssl s_client exited with code ${code}\n${stderr}`);

// Verify the reply from Node's server appeared in s_client's stdout.
assert(stdout.includes(reply),
       `Expected stdout to include "${reply}"\n${stdout}`);

// Verify it was a DTLS connection.
assert(stdout.includes('DTLS'),
       `Expected stdout to include "DTLS"\n${stdout}`);

await endpoint.close();
