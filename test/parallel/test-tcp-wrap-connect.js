'use strict';
require('../common');
const assert = require('assert');
const TCP = process.binding('tcp_wrap').TCP;
const TCPConnectWrap = process.binding('tcp_wrap').TCPConnectWrap;
const ShutdownWrap = process.binding('stream_wrap').ShutdownWrap;

function makeConnection() {
  const client = new TCP();

  const req = new TCPConnectWrap();
  const err = client.connect(req, '127.0.0.1', this.address().port);
  assert.strictEqual(err, 0);

  req.oncomplete = function(status, client_, req_, readable, writable) {
    assert.strictEqual(0, status);
    assert.strictEqual(client, client_);
    assert.strictEqual(req, req_);
    assert.strictEqual(true, readable);
    assert.strictEqual(true, writable);

    const shutdownReq = new ShutdownWrap();
    const err = client.shutdown(shutdownReq);
    assert.strictEqual(err, 0);

    shutdownReq.oncomplete = function(status, client_, req_) {
      assert.strictEqual(0, status);
      assert.strictEqual(client, client_);
      assert.strictEqual(shutdownReq, req_);
      shutdownCount++;
      client.close();
    };
  };
}

/////

let connectCount = 0;
let endCount = 0;
let shutdownCount = 0;

const server = require('net').Server(function(s) {
  connectCount++;
  s.resume();
  s.on('end', function() {
    endCount++;
    s.destroy();
    server.close();
  });
});

server.listen(0, makeConnection);

process.on('exit', function() {
  assert.strictEqual(1, shutdownCount);
  assert.strictEqual(1, connectCount);
  assert.strictEqual(1, endCount);
});
