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
  assert.equal(err, 0);

  req.oncomplete = function(status, client_, req_) {
    assert.equal(0, status);
    assert.equal(client, client_);
    assert.equal(req, req_);

    console.log('connected');
    var shutdownReq = new ShutdownWrap();
    var err = client.shutdown(shutdownReq);
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

var connectCount = 0;
var endCount = 0;
var shutdownCount = 0;

var server = require('net').Server(function(s) {
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
