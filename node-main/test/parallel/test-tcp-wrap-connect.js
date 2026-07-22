// Flags: --expose-internals
'use strict';
require('../common');
const assert = require('assert');
const { internalBinding } = require('internal/test/binding');
const {
  TCP,
  constants: TCPConstants,
  TCPConnectWrap
} = internalBinding('tcp_wrap');
const { ShutdownWrap } = internalBinding('stream_wrap');

function makeConnection() {
  const client = new TCP(TCPConstants.SOCKET);

  const req = new TCPConnectWrap();
  const err = client.connect(req, '127.0.0.1', this.address().port);
  assert.strictEqual(err, 0);

  req.oncomplete = function(status, client_, req_, readable, writable) {
    assert.strictEqual(status, 0);
    assert.strictEqual(client_, client);
    assert.strictEqual(req_, req);
    assert.strictEqual(readable, true);
    assert.strictEqual(writable, true);

    const shutdownReq = new ShutdownWrap();
    const err = client.shutdown(shutdownReq);
    assert.strictEqual(err, 0);

    shutdownReq.oncomplete = function(status, client_, error) {
      assert.strictEqual(status, 0);
      assert.strictEqual(client_, client);
      assert.strictEqual(error, undefined);
      shutdownCount++;
      client.close();
    };
  };
}

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
  assert.strictEqual(shutdownCount, 1);
  assert.strictEqual(connectCount, 1);
  assert.strictEqual(endCount, 1);
});
