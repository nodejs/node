'use strict';

// This test verifies the guards that reject transferring a net.Socket,
// net.Server or net.BoundSocket that is not in a clean, movable state.
// Serialization is triggered with a MessageChannel, so no worker thread is
// required.

const common = require('../common');

const assert = require('assert');
const net = require('net');
const { MessageChannel } = require('worker_threads');
const tmpdir = require('../common/tmpdir');

tmpdir.refresh();

const { port1 } = new MessageChannel();

// A pipe BoundSocket is not transferable; only TCP binds can move between
// event loops.
{
  const bound = new net.BoundSocket({ path: common.PIPE });
  assert.throws(() => port1.postMessage({ bound }, [bound]), {
    code: 'ERR_WORKER_HANDLE_NOT_TRANSFERABLE',
  });
  bound.close();
}

// A Server that is not listening (no handle) cannot be transferred.
{
  const server = net.createServer();
  assert.throws(() => port1.postMessage({ server }, [server]), {
    code: 'ERR_WORKER_HANDLE_NOT_TRANSFERABLE',
  });
}

// A closed BoundSocket no longer owns a handle and cannot be transferred.
{
  const bound = new net.BoundSocket({ host: '127.0.0.1', port: 0 });
  bound.close();
  assert.throws(() => port1.postMessage({ bound }, [bound]), {
    code: 'ERR_WORKER_HANDLE_NOT_TRANSFERABLE',
  });
}

// An adopted BoundSocket no longer owns a handle and cannot be transferred.
{
  const bound = new net.BoundSocket({ host: '127.0.0.1', port: 0 });
  const socket = new net.Socket({ handle: bound });
  assert.throws(() => port1.postMessage({ bound }, [bound]), {
    code: 'ERR_WORKER_HANDLE_NOT_TRANSFERABLE',
  });
  socket.destroy();
}

// A Socket that has already consumed data cannot be transferred, because that
// buffered data would be lost on the sending side.
{
  const server = net.createServer(common.mustCall((socket) => {
    socket.on('data', common.mustCall(() => {
      assert.throws(() => port1.postMessage({ socket }, [socket]), {
        code: 'ERR_WORKER_HANDLE_NOT_TRANSFERABLE',
      });
      // The socket is still usable after a rejected transfer.
      assert.ok(socket._handle != null);
      socket.destroy();
      server.close();
      port1.close();
    }));
  }));

  server.listen(0, common.mustCall(() => {
    const client = net.connect(server.address().port, common.mustCall(() => {
      client.end('data');
    }));
    // The server destroys the socket after the rejected transfer, which may
    // surface as a reset on the client; swallow it either way.
    client.on('error', () => {});
  }));
}
