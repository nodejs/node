'use strict';
const common = require('../common');
const net = require('net');
const assert = require('assert');

const server = net.createServer();
server.listen(0, common.mustCall(function() {
  const port = server.address().port;
  const conn = net.createConnection(port);
  server.on('connection', (socket) => {
    socket.on('error', common.expectsError({
      code: 'ECONNRESET',
      message: 'read ECONNRESET',
      name: 'Error'
    }));
  });

  conn.on('connect', common.mustCall(function() {
    assert.strictEqual(conn, conn.resetAndDestroy().destroy());
    conn.on('error', common.mustNotCall());

    conn.write(Buffer.from('fzfzfzfzfz'), common.expectsError({
      code: 'ERR_STREAM_DESTROYED',
      message: 'Cannot call write after a stream was destroyed',
      name: 'Error'
    }));
    server.close();
  }));
}));
