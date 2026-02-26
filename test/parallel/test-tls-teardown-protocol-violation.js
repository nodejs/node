'use strict';

const common = require('../common');
if (!common.hasCrypto)
  common.skip('missing crypto');

const assert = require('assert');
const { spawnSync } = require('child_process');

if (process.argv[2] === 'child') {
  const mode = process.argv[3];
  const fixtures = require('../common/fixtures');
  const tls = require('tls');
  const { duplexPair } = require('stream');

  const key = fixtures.readKey('agent1-key.pem');
  const cert = fixtures.readKey('agent1-cert.pem');
  const ca = fixtures.readKey('ca1-cert.pem');
  const [clientSide, serverSide] = duplexPair();

  const client = tls.connect({
    socket: clientSide,
    ca,
    host: 'agent1',
  });
  const server = new tls.TLSSocket(serverSide, {
    isServer: true,
    key,
    cert,
    ca,
  });

  const timeout = setTimeout(() => {
    console.error(`timeout:${mode}`);
    process.exit(2);
  }, 3000);

  switch (mode) {
    case 'teardown-protocol-violation':
      client.on('secureConnect', () => {
        client.end();
      });
      server.on('end', () => {
        // Inject raw plaintext after receiving close_notify.
        serverSide.end('221 closing connection\r\n');
      });
      // The regression here is an unhandled 'error' crash. If we reach this
      // timer callback, teardown handled the protocol violation gracefully.
      setTimeout(() => {
        clearTimeout(timeout);
        process.exit(0);
      }, 200);
      break;
    case 'pre-teardown-protocol-violation':
      client.on('secureConnect', () => {
        serverSide.end('221 closing connection\r\n');
      });
      break;
    case 'graceful-close':
      client.on('secureConnect', () => {
        client.end();
      });
      server.on('end', () => {
        server.end();
      });
      client.on('close', () => {
        clearTimeout(timeout);
        process.exit(0);
      });
      break;
    default:
      throw new Error(`Unknown mode: ${mode}`);
  }

  return;
}

function runCase(mode) {
  const result = spawnSync(process.execPath, [__filename, 'child', mode], {
    encoding: 'utf8',
  });

  return {
    code: result.status,
    signal: result.signal,
    stderr: result.stderr,
    stdout: result.stdout,
  };
}

{
  const result = runCase('teardown-protocol-violation');
  assert.strictEqual(result.code, 0, result.stderr);
  assert.strictEqual(result.signal, null, result.stderr);
  assert.doesNotMatch(result.stderr, /Unhandled 'error' event/);
}

{
  const result = runCase('pre-teardown-protocol-violation');
  assert.strictEqual(result.code, 1, result.stderr);
  assert.match(result.stderr, /Unhandled 'error' event/);
  assert.match(result.stderr, /Emitted 'error' event on TLSSocket instance/);
}

{
  const result = runCase('graceful-close');
  assert.strictEqual(result.code, 0, result.stderr);
  assert.strictEqual(result.signal, null, result.stderr);
  assert.strictEqual(result.stderr, '');
}
