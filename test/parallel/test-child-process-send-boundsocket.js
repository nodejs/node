'use strict';

// This test verifies that an un-adopted TCP net.BoundSocket can be sent to a
// child process as the sendHandle argument of subprocess.send(). The parent
// binds synchronously to reserve the port, then hands the bound socket off;
// the child adopts it via server.listen() and accepts connections.

const common = require('../common');

const assert = require('assert');
const net = require('net');
const { fork } = require('child_process');

if (process.argv[2] === 'child') {
  process.on('message', common.mustCall((msg, bound) => {
    assert.ok(bound instanceof net.BoundSocket);
    // The bound address survives the send.
    assert.strictEqual(bound.address().port, msg.port);

    const server = net.createServer(common.mustCall((socket) => {
      socket.end('from-child');
      server.close();
    }));
    server.listen(bound, common.mustCall(() => {
      assert.strictEqual(server.address().port, msg.port);
      // Adoption consumed the bound socket.
      assert.throws(() => bound.address(), {
        code: 'ERR_SOCKET_HANDLE_ADOPTED',
      });
      process.send('listening');
    }));
  }));
  return;
}

const tmpdir = require('../common/tmpdir');
tmpdir.refresh();

const child = fork(__filename, ['child']);

// Guards throw synchronously while no handle send is in flight (queued sends
// defer conversion).
const pipeBound = new net.BoundSocket({ path: common.PIPE });
assert.throws(() => child.send('x', pipeBound), {
  code: 'ERR_INVALID_HANDLE_TYPE',
});
pipeBound.close();

const closedBound = new net.BoundSocket({ host: '127.0.0.1', port: 0 });
closedBound.close();
assert.throws(() => child.send('x', closedBound), {
  code: 'ERR_WORKER_HANDLE_NOT_TRANSFERABLE',
});

const bound = new net.BoundSocket({ host: '127.0.0.1', port: 0 });
const { port } = bound.address();

child.send({ port }, bound, common.mustSucceed());

// The source is left in the adopted state; further use throws.
assert.throws(() => bound.address(), { code: 'ERR_SOCKET_HANDLE_ADOPTED' });
assert.throws(() => bound.fd(), { code: 'ERR_SOCKET_HANDLE_ADOPTED' });
assert.throws(() => bound.close(), { code: 'ERR_SOCKET_HANDLE_ADOPTED' });

child.on('message', common.mustCall((msg) => {
  assert.strictEqual(msg, 'listening');
  const client = net.connect(port, '127.0.0.1');
  client.setEncoding('utf8');
  let response = '';
  client.on('data', (chunk) => { response += chunk; });
  client.on('end', common.mustCall(() => {
    assert.strictEqual(response, 'from-child');
    child.disconnect();
  }));
}));

child.on('exit', common.mustCall((code) => {
  assert.strictEqual(code, 0);
}));
