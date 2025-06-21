// Flags: --expose-internals
'use strict';
const common = require('../common');
const assert = require('assert');

const { internalBinding } = require('internal/test/binding');
const { TCP, constants: TCPConstants } = internalBinding('tcp_wrap');
const {
  WriteWrap,
  kReadBytesOrError,
  kArrayBufferOffset,
  streamBaseState
} = internalBinding('stream_wrap');

const server = new TCP(TCPConstants.SOCKET);

const r = (common.hasIPv6 ?
  server.bind6('::', 0) :
  server.bind('0.0.0.0', 0));
assert.strictEqual(r, 0);

const port = {};
server.getsockname(port);

server.listen(128);

server.onconnection = (err, client) => {
  assert.strictEqual(client.writeQueueSize, 0);
  console.log('got connection');

  const maybeCloseClient = () => {
    if (client.pendingWrites.length === 0 && client.gotEOF) {
      console.log('close client');
      client.close();
    }
  };

  client.readStart();
  client.pendingWrites = [];
  client.onread = common.mustCall((arrayBuffer) => {
    if (arrayBuffer) {
      const offset = streamBaseState[kArrayBufferOffset];
      const nread = streamBaseState[kReadBytesOrError];
      const buffer = Buffer.from(arrayBuffer, offset, nread);
      assert.ok(buffer.length > 0);

      assert.strictEqual(client.writeQueueSize, 0);

      const req = new WriteWrap();
      req.async = false;
      const returnCode = client.writeBuffer(req, buffer);
      assert.strictEqual(returnCode, 0);
      client.pendingWrites.push(req);

      console.log(`client.writeQueueSize: ${client.writeQueueSize}`);
      // 11 bytes should flush
      assert.strictEqual(client.writeQueueSize, 0);

      if (req.async)
        req.oncomplete = common.mustCall(done);
      else
        process.nextTick(done.bind(null, 0, client, req));

      function done(status, client_, req_) {
        assert.strictEqual(client.pendingWrites.shift(), req);

        // Check parameters.
        assert.strictEqual(status, 0);
        assert.strictEqual(client_, client);
        assert.strictEqual(req_, req);

        console.log(`client.writeQueueSize: ${client.writeQueueSize}`);
        assert.strictEqual(client.writeQueueSize, 0);

        maybeCloseClient();
      }

    } else {
      console.log('eof');
      client.gotEOF = true;
      server.close();
      maybeCloseClient();
    }
  }, 2);
};

const net = require('net');

const c = net.createConnection(port);

c.on('connect', common.mustCall(() => { c.end('hello world'); }));

c.setEncoding('utf8');
c.on('data', common.mustCall((d) => {
  assert.strictEqual(d, 'hello world');
}));

c.on('close', () => {
  console.error('client closed');
});
