// Flags: --expose-internals
'use strict';
const common = require('../common');
const assert = require('assert');
const net = require('net');
const { internalBinding } = require('internal/test/binding');
const TCPWrap = internalBinding('tcp_wrap').TCP;

const echoServer = net.createServer(function(connection) {
  connection.end();
});
echoServer.listen(0);

let callCount = 0;

const Socket = net.Socket;
const setNoDelay = TCPWrap.prototype.setNoDelay;

TCPWrap.prototype.setNoDelay = function(enable) {
  setNoDelay.call(this, enable);
  callCount++;
};

echoServer.on('listening', common.mustCall(function() {
  const sock1 = new Socket();
  // setNoDelay before the handle is created
  // there is probably a better way to test this

  const s = sock1.setNoDelay();
  assert.ok(s instanceof net.Socket);
  sock1.connect(this.address().port);
  sock1.on('end', common.mustCall(() => {
    assert.strictEqual(callCount, 1);
    echoServer.close();
  }));
}));
