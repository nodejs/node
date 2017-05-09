'use strict';
const common = require('../common');
const assert = require('assert');

const TCP = process.binding('tcp_wrap').TCP;
const WriteWrap = process.binding('stream_wrap').WriteWrap;

const server = new TCP();

const r = server.bind('0.0.0.0', 0);
assert.strictEqual(0, r);
let port = {};
server.getsockname(port);
port = port.port;

server.listen(128);

server.onconnection = (err, client) => {
  assert.strictEqual(0, client.writeQueueSize);
  console.log('got connection');

  const maybeCloseClient = () => {
    if (client.pendingWrites.length === 0 && client.gotEOF) {
      console.log('close client');
      client.close();
    }
  };

  client.readStart();
  client.pendingWrites = [];
  client.onread = common.mustCall((err, buffer) => {
    if (buffer) {
      assert.ok(buffer.length > 0);

      assert.strictEqual(0, client.writeQueueSize);

      const req = new WriteWrap();
      req.async = false;
      const returnCode = client.writeBuffer(req, buffer);
      assert.strictEqual(returnCode, 0);
      client.pendingWrites.push(req);

      console.log(`client.writeQueueSize: ${client.writeQueueSize}`);
      // 11 bytes should flush
      assert.strictEqual(0, client.writeQueueSize);

      if (req.async)
        req.oncomplete = common.mustCall(done);
      else
        process.nextTick(done.bind(null, 0, client, req));

      function done(status, client_, req_) {
        assert.strictEqual(req, client.pendingWrites.shift());

        // Check parameters.
        assert.strictEqual(0, status);
        assert.strictEqual(client, client_);
        assert.strictEqual(req, req_);

        console.log(`client.writeQueueSize: ${client.writeQueueSize}`);
        assert.strictEqual(0, client.writeQueueSize);

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
  assert.strictEqual('hello world', d);
}));

c.on('close', () => {
  console.error('client closed');
});
