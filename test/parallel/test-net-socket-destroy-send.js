'use strict';

const common = require('../common');
const net = require('net');
const assert = require('assert');

const server = net.createServer();
server.listen(0, common.mustCall(function() {
  const port = server.address().port;
  const conn = net.createConnection(port);

  conn.on('connect', common.mustCall(function() {
    // Test destroy returns this, even on multiple calls when it short-circuits.
    assert.strictEqual(conn, conn.destroy().destroy());
    conn.on('error', common.expectsError({
      code: 'ERR_STREAM_DESTROYED',
      message: 'Cannot call write after a stream was destroyed',
      type: Error
    }));

    conn.write(Buffer.from('kaboom'), common.expectsError({
      code: 'ERR_STREAM_DESTROYED',
      message: 'Cannot call write after a stream was destroyed',
      type: Error
    }));
    server.close();
  }));
}));
