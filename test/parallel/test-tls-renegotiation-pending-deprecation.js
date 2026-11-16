'use strict';

const common = require('../common');
if (!common.hasCrypto)
  common.skip('missing crypto');

const { isBoringSSL } = require('../common/crypto');
if (isBoringSSL)
  common.skip('BoringSSL does not support renegotiation');

// With --pending-deprecation, renegotiation emits a deprecation warning unless
// it is a client that set allowRenegotiation: true accepting a server's request.

const assert = require('assert');
const { fork } = require('child_process');
const tls = require('tls');
const fixtures = require('../common/fixtures');

tls.DEFAULT_MAX_VERSION = 'TLSv1.2';

const serverOptions = {
  key: fixtures.readKey('rsa_private.pem'),
  cert: fixtures.readKey('rsa_cert.crt'),
};

// The endpoint under test runs in a child process, as the warning is emitted
// once per process and the peer would otherwise emit it too.
if (process.argv[2] === 'child') {
  const { role, allowRenegotiation, initiate, port } =
    JSON.parse(process.argv[3]);

  if (role === 'server') {
    const server = tls.createServer({
      ...serverOptions,
      allowRenegotiation,
    }, common.mustCall((socket) => {
      socket.resume();
      socket.on('close', common.mustCall(() => server.close()));
    }));
    server.listen(0, common.mustCall(() => {
      process.send({ port: server.address().port });
    }));
  } else {
    const client = tls.connect({
      port,
      rejectUnauthorized: false,
      allowRenegotiation,
    }, common.mustCall(() => {
      if (initiate)
        client.renegotiate({}, common.mustSucceed(() => client.end()));
    }));
    client.resume();
  }
  return;
}

function start(options, warns) {
  const child = fork(__filename, ['child', JSON.stringify(options)], {
    execArgv: ['--pending-deprecation'],
    stdio: ['ignore', 'inherit', 'pipe', 'ipc'],
  });
  let stderr = '';
  child.stderr.setEncoding('utf8');
  child.stderr.on('data', (chunk) => { stderr += chunk; });
  child.on('close', common.mustCall((code) => {
    assert.strictEqual(code, 0, stderr);
    assert.strictEqual(
      stderr.includes('[DEP0213] DeprecationWarning: TLS renegotiation'),
      warns,
      stderr);
  }));
  return child;
}

function testServer(options, warns) {
  const child = start({ role: 'server', ...options }, warns);
  child.on('message', common.mustCall(({ port }) => {
    const client = tls.connect({
      port,
      rejectUnauthorized: false,
    }, common.mustCall(() => {
      client.renegotiate({}, common.mustSucceed(() => client.end()));
    }));
    client.resume();
  }));
}

function testClient(options, warns) {
  const server = tls.createServer(serverOptions, common.mustCall((socket) => {
    socket.resume();
    socket.on('close', common.mustCall(() => server.close()));
    if (!options.initiate)
      socket.renegotiate({}, common.mustSucceed(() => socket.end()));
  }));
  server.listen(0, common.mustCall(() => {
    start({ role: 'client', port: server.address().port, ...options }, warns);
  }));
}

// A server completing a renegotiation, whether or not it set the option.
testServer({ allowRenegotiation: true }, true);
// Any call to renegotiate().
testClient({ allowRenegotiation: true, initiate: true }, true);
// A client accepting the server's request, unless it set the option.
testClient({ initiate: false }, true);
testClient({ allowRenegotiation: true, initiate: false }, false);
