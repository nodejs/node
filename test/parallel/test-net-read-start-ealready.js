// Flags: --expose-internals
'use strict';

const common = require('../common');
const assert = require('assert');
const net = require('net');
const { internalBinding } = require('internal/test/binding');
const { UV_EALREADY } = internalBinding('uv');

// Close the server from the accepted connection rather than from the client's
// 'close'. Closing the listening handle can race ahead of the accept callback
// and drop the pending connection, so 'connection' would never fire.
const server = net.createServer(common.mustCall((connection) => {
  connection.on('error', common.mustNotCall());
  connection.resume();
  connection.on('close', common.mustCall(() => server.close()));
}));

server.listen(0, '127.0.0.1', common.mustCall(() => {
  const socket = net.createConnection({
    host: '127.0.0.1',
    port: server.address().port,
  });

  socket.on('connect', common.mustCall(() => {
    const readStart = socket._handle.readStart;
    socket._handle.readStart = () => UV_EALREADY;
    socket._handle.reading = false;

    socket._read(0);

    assert.strictEqual(socket.destroyed, false);
    assert.notStrictEqual(socket._handle, null);

    socket._handle.readStart = readStart;
    socket.destroy();
  }));

  socket.on('error', common.mustNotCall());
  socket.on('close', common.mustCall());
}));
