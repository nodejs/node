var common = require('../common');
var assert = require('assert');
var net = require('net');
var TCPWrap = process.binding('tcp_wrap').TCP;

var echoServer = net.createServer(function(conn) {
  conn.end();
});

var ref = TCPWrap.prototype.ref;
var unref = TCPWrap.prototype.unref;

var refed = false;
var unrefed = false;

TCPWrap.prototype.ref = function() {
  ref.call(this);
  refed = true;
};

TCPWrap.prototype.unref = function() {
  unref.call(this);
  unrefed = true;
};

echoServer.listen(common.PORT);

echoServer.on('listening', function() {
  var sock = new net.Socket();
  sock.unref();
  sock.ref();
  sock.connect(common.PORT);
  sock.on('end', process.exit);
});

process.on('exit', function() {
  assert.ok(refed);
  assert.ok(unrefed);
});
