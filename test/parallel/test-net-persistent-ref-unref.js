'use strict';
var common = require('../common');
var assert = require('assert');
var net = require('net');
var TCPWrap = process.binding('tcp_wrap').TCP;

var echoServer = net.createServer(function(conn) {
  conn.end();
});

var ref = TCPWrap.prototype.ref;
var unref = TCPWrap.prototype.unref;

var refCount = 0;

TCPWrap.prototype.ref = function() {
  ref.call(this);
  refCount++;
  assert.equal(refCount, 0);
};

TCPWrap.prototype.unref = function() {
  unref.call(this);
  refCount--;
  assert.equal(refCount, -1);
};

echoServer.listen(common.PORT);

echoServer.on('listening', function() {
  var sock = new net.Socket();
  sock.unref();
  sock.ref();
  sock.connect(common.PORT);
  sock.on('end', function() {
    assert.equal(refCount, 0);
    echoServer.close();
  });
});
