'use strict';
const common = require('../common');
const assert = require('assert');
const net = require('net');

{
  const server = net.createServer();

  server.listen(common.mustCall(() => {
    const port = server.address().port;
    const client = net.connect({ port }, common.mustCall(() => {
      client.on('error', common.mustCall((err) => {
        server.close();
        assert.strictEqual(err.constructor, Error);
        assert.strictEqual(err.message, 'write EBADF');
      }));
      client._handle.close();
      client.write('foo');
    }));
  }));
}

{
  const server = net.createServer();

  server.listen(common.mustCall(() => {
    const port = server.address().port;
    const client = net.connect({ port }, common.mustCall(() => {
      client.on('error', common.expectsError({
        code: 'ERR_SOCKET_CLOSED',
        message: 'Socket is closed',
        type: Error
      }));

      server.close();

      client._handle.close();
      client._handle = null;
      client.write('foo');
    }));
  }));
}
