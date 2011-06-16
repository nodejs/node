var common = require('../common');
var assert = require('assert');
var TCP = process.binding('tcp_wrap').TCP;

function makeConnection() {
  var client = new TCP();

  var req = client.connect('127.0.0.1', common.PORT);
  req.oncomplete = function(status, client_, req_) {
    assert.equal(0, status);
    assert.equal(client, client_);
    assert.equal(req, req_);

    console.log("connected");
    var shutdownReq = client.shutdown();
    shutdownReq.oncomplete = function(status, client_, req_) {
      console.log("shutdown complete");
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
  console.log("got connection");
  connectCount++;
  s.on('end', function() {
    console.log("got eof");
    endCount++;
    s.destroy();
    server.close();
  });
});

server.listen(common.PORT, makeConnection);

process.on('exit', function() {
  assert.equal(1, shutdownCount);
  assert.equal(1, connectCount);
  assert.equal(1, endCount);
});
