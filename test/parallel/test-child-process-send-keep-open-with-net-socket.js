// Flags: --expose-internals
'use strict';
const common = require('../common');
const assert = require('assert');
const cp = require('child_process');
const net = require('net');
const { internalBinding } = require('internal/test/binding');
const { TCP } = internalBinding('tcp_wrap');

const count = 6;

if (process.argv[2] !== 'child') {
  const child = cp.fork(__filename, ['child']);

  let socket1, socket2, socket3, socket4, socket5, socket6;
  function noop() {}
  // The first step
  const server = net.createServer(common.mustCall(noop, count))
    .listen(() => {
      const address = server.address();
      socket1 = net.connect(address.port, address.address, common.mustCall(() => {
        child.send('socket', socket1, { keepOpen: true }, common.mustSucceed());
      }));
      socket2 = net.connect(address.port, address.address, common.mustCall(() => {
        child.send('socket', socket2, { keepOpen: false }, common.mustSucceed());
      }));
      socket3 = net.connect(address.port, address.address, common.mustCall(() => {
        // The default value of `keepOpen` is false
        child.send('socket', socket3, common.mustSucceed());
      }));
      socket4 = net.connect(address.port, address.address, common.mustCall(() => {
        child.send('handle', socket4._handle, { keepOpen: true }, common.mustSucceed());
      }));
      socket5 = net.connect(address.port, address.address, common.mustCall(() => {
        child.send('handle', socket5._handle, { keepOpen: false }, common.mustSucceed());
      }));
      socket6 = net.connect(address.port, address.address, common.mustCall(() => {
        // The default value of `keepOpen` is true
        child.send('handle', socket6._handle, common.mustSucceed());
      }));
    });

  let i = 0;
  // The third step
  child.on('message', common.mustCall((msg) => {
    assert.strictEqual(msg, 'done');
    if (++i === count) {
      child.disconnect();
    }
  }, count));

  // The fourth step
  child.on('exit', common.mustCall((code) => {
    assert.strictEqual(code, 0);
    // Test if the socket is closed
    assert.strictEqual(!!socket1.localPort, true);
    assert.strictEqual(!!socket2.localPort, false);
    assert.strictEqual(!!socket3.localPort, false);
    assert.strictEqual(!!socket4.localPort, true);
    assert.strictEqual(!!socket5.localPort, false);
    assert.strictEqual(!!socket6.localPort, true);
    socket1.destroy();
    socket4.destroy();
    socket6.destroy();
    server.close();
  }));
} else {
  // The second step
  process.on('message', common.mustCall((_, handle) => {
    if (handle instanceof TCP) {
      handle.close(common.mustCall(() => {
        process.send('done');
      }));
    } else if (handle instanceof net.Socket) {
      handle.destroy();
      handle.on('close', common.mustCall(() => {
        process.send('done');
      }));
    } else {
      throw new Error('invalid type');
    }
  }, count));
}
