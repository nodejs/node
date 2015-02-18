var common = require('../common');
var assert = require('assert');
var net = require('net');
var TCPWrap = process.binding('tcp_wrap').TCP;

var echoServer = net.createServer(function(connection) {
  connection.end();
});
echoServer.listen(common.PORT);

var setTrue = 0;

var Socket = net.Socket;
var setNoDelay = TCPWrap.prototype.setNoDelay;

TCPWrap.prototype.setNoDelay = function(enable) {
  setNoDelay.call(this, enable);
  setTrue++;
};

echoServer.on('listening', function() {
  var sock1 = new Socket();
  // setNoDelay before the handle is created
  // there is probably a better way to test this

  sock1.setNoDelay();
  sock1.connect(common.PORT);
  sock1.on('end', process.exit);
});

process.on('exit', function() {
  assert.ok(setTrue);
});
