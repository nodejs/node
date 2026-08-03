// Flags: --expose-internals
'use strict';
require('../common');
const assert = require('assert');
const net = require('net');
const { internalBinding } = require('internal/test/binding');
const TCPWrap = internalBinding('tcp_wrap').TCP;

const echoServer = net.createServer((conn) => {
  conn.end();
});

const ref = TCPWrap.prototype.ref;
const unref = TCPWrap.prototype.unref;

let refCount = 0;

TCPWrap.prototype.ref = function() {
  ref.call(this);
  refCount++;
  assert.strictEqual(refCount, 0);
};

TCPWrap.prototype.unref = function() {
  unref.call(this);
  refCount--;
  assert.strictEqual(refCount, -1);
};

echoServer.listen(0);

echoServer.on('listening', function() {
  const sock = new net.Socket();
  sock.unref();
  sock.ref();
  sock.connect(this.address().port);
  sock.on('end', () => {
    assert.strictEqual(refCount, 0);
    echoServer.close();
  });
});
