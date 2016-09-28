'use strict';
require('../common');
var assert = require('assert');
var TCP = process.binding('tcp_wrap').TCP;
var TCPConnectWrap = process.binding('tcp_wrap').TCPConnectWrap;
var ShutdownWrap = process.binding('stream_wrap').ShutdownWrap;

function makeConnection() {
  var client = new TCP();

  var req = new TCPConnectWrap();
  var err = client.connect(req, '127.0.0.1', this.address().port);
  assert.strictEqual(err, 0);

  req.oncomplete = function(status, client_, req_, readable, writable) {
    assert.strictEqual(0, status);
    assert.strictEqual(client, client_);
    assert.strictEqual(req, req_);
    assert.strictEqual(true, readable);
    assert.strictEqual(true, writable);

    var shutdownReq = new ShutdownWrap();
    var err = client.shutdown(shutdownReq);
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

var connectCount = 0;
var endCount = 0;
var shutdownCount = 0;

var server = require('net').Server(function(s) {
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
