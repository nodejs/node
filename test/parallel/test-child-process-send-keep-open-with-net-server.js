'use strict';
const common = require('../common');
const assert = require('assert');
const cp = require('child_process');
const net = require('net');

const count = 3;

if (process.argv[2] !== 'child') {
  const child = cp.fork(__filename, ['child']);
  // The first step
  const server1 = net.createServer(common.mustNotCall())
    .listen(() => {
      child.send(null, server1, { keepOpen: true }, common.mustSucceed());
    });

  const server2 = net.createServer(common.mustNotCall())
    .listen(() => {
      child.send(null, server2, { keepOpen: false }, common.mustSucceed());
    });

  const server3 = net.createServer(common.mustNotCall())
    .listen(() => {
      // The default value of `keepOpen` is true
      child.send(null, server3, common.mustSucceed());
    });

  let i = 0;

  // The third step
  child.on('message', common.mustCall(() => {
    if (++i === count) {
      child.disconnect();
    }
  }, count));

  // The fourth step
  child.on('exit', common.mustCall((code) => {
    assert.strictEqual(code, 0);
    // Test if the server is closed
    assert.strictEqual(server1.address().port > 0, true);
    assert.throws(() => server2.address(), Error);
    assert.strictEqual(server3.address().port > 0, true);
    server1.close();
    server3.close();
  }));
} else {
  // The second step
  process.on('message', common.mustCall((_, server) => {
    assert.strictEqual(server instanceof net.Server, true);
    server.close(common.mustCall(() => {
      process.send('done');
    }));
  }, count));
}
