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
  assert.equal(err, 0);

  req.oncomplete = function(status, client_, req_) {
    assert.equal(0, status);
    assert.equal(client, client_);
    assert.equal(req, req_);

    console.log('connected');
    const shutdownReq = new ShutdownWrap();
    const err = client.shutdown(shutdownReq);
    assert.equal(err, 0);

    shutdownReq.oncomplete = function(status, client_, req_) {
      console.log('shutdown complete');
      assert.equal(0, status);
      assert.equal(client, client_);
      assert.equal(shutdownReq, req_);
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
  console.log('got connection');
  connectCount++;
  s.resume();
  s.on('end', function() {
    console.log('got eof');
    endCount++;
    s.destroy();
    server.close();
  });
});

server.listen(0, makeConnection);

process.on('exit', function() {
  assert.equal(1, shutdownCount);
  assert.equal(1, connectCount);
  assert.equal(1, endCount);
});
