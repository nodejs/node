// Flags: --experimental-dtls --no-warnings

// Test: DTLS interop -- OpenSSL s_server with Node.js DTLS client.
// Verifies that Node's DTLS client can complete a handshake with an
// external DTLS server (OpenSSL CLI) and exchange application data.

import { hasCrypto, skip, mustCall, mustNotCall } from '../common/index.mjs';
import { createRequire } from 'module';
import assert from 'node:assert';
import { spawn } from 'node:child_process';
import * as fixtures from '../common/fixtures.mjs';

if (!hasCrypto) {
  skip('missing crypto');
}

if (!process.features.dtls) {
  skip('DTLS is not enabled');
}

const require = createRequire(import.meta.url);
const common = require('../common');
const { opensslCli } = require('../common/crypto');

if (!opensslCli) {
  skip('missing openssl-cli');
}

const { connect } = await import('node:dtls');

const reply = 'I AM THE WALRUS'; // Something recognizable

// Start OpenSSL DTLS server.
const server = spawn(opensslCli, [
  's_server',
  '-dtls1_2',
  '-accept', String(common.PORT),
  '-cert', fixtures.path('keys/agent1-cert.pem'),
  '-key', fixtures.path('keys/agent1-key.pem'),
  '-listen',
], { stdio: 'pipe' });

let serverOut = '';
server.stdout.on('data', (data) => { serverOut += data; });
let serverErr = '';
server.stderr.on('data', (data) => { serverErr += data; });
server.on('error', mustNotCall());

const timeout = setTimeout(() => {
  server.kill();
  assert.fail(`Test timed out\nstdout: ${serverOut}\nstderr: ${serverErr}`);
}, 10000);

// Wait for "ACCEPT" on stdout -- this means s_server is ready.
await new Promise((resolve) => {
  server.stdout.on('data', function onReady() {
    if (!serverOut.includes('ACCEPT')) return;
    server.stdout.removeListener('data', onReady);
    resolve();
  });
});

// Connect Node.js DTLS client.
const session = connect('127.0.0.1', common.PORT, {
  ca: [fixtures.readKey('ca1-cert.pem').toString()],
  rejectUnauthorized: false,
});

const { protocol } = await session.opened;
assert.match(protocol, /DTLS/i);

// Send data from Node to OpenSSL server.
session.send('hello from node');

// Send data from OpenSSL server to Node client via s_server stdin.
// s_server forwards its stdin to the connected client.
server.stdin.write(reply + '\n');

// Wait for Node client to receive the message.
const data = await new Promise((resolve) => {
  session.onmessage = mustCall(resolve);
});
assert.strictEqual(data.toString().trim(), reply);

// Clean up.
await session.close();
await session.endpoint.close();
clearTimeout(timeout);
server.kill();

// Wait for server to exit.
const [, signal] = await new Promise((resolve) => {
  server.on('exit', mustCall((...args) => resolve(args)));
});
assert.strictEqual(signal, 'SIGTERM');
