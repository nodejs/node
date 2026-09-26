'use strict';

const common = require('../common');
const assert = require('node:assert');
const { createConnection, createServer } = require('node:net');
const { TCP } = process.binding('tcp_wrap');

// Delay delivery of a successful TCP completion until the other address has
// failed. The first connection must remain usable after fallback has started.
const connect = TCP.prototype.connect;
let firstCompletion;
let secondFailed = false;
TCP.prototype.connect = function(req, address, port) {
  if (address === '127.0.0.1') {
    const oncomplete = req.oncomplete;
    req.oncomplete = (...args) => {
      const complete = () => oncomplete(...args);
      if (secondFailed) {
        setImmediate(complete);
      } else {
        firstCompletion = complete;
      }
    };
  }
  return Reflect.apply(connect, this, [req, address, port]);
};

const server = createServer(common.mustCall((socket) => socket.end()));
server.listen(0, '127.0.0.1', common.mustCall(() => {
  const port = server.address().port;
  const secondAddress = common.hasIPv6 ? '::1' : '127.0.0.2';
  const connection = createConnection({
    host: 'example.org',
    port,
    lookup: common.mustCall((host, options, callback) => {
      assert.strictEqual(options.all, true);
      process.nextTick(callback, null, [
        { address: '127.0.0.1', family: 4 },
        { address: secondAddress, family: common.hasIPv6 ? 6 : 4 },
      ]);
    }),
    autoSelectFamily: true,
    autoSelectFamilyAttemptTimeout: 10,
  });

  connection.on('connectionAttemptTimeout', common.mustCall((address) => {
    assert.strictEqual(address, '127.0.0.1');
  }));
  connection.on('connectionAttemptFailed', common.mustCall((address) => {
    assert.strictEqual(address, secondAddress);
    secondFailed = true;
    if (firstCompletion) setImmediate(firstCompletion);
  }));
  connection.on('connect', common.mustCall(() => {
    assert.strictEqual(connection.remoteAddress, '127.0.0.1');
    assert.deepStrictEqual(connection.autoSelectFamilyAttemptedAddresses,
                           [`127.0.0.1:${port}`, `${secondAddress}:${port}`]);
  }));
  connection.on('error', common.mustNotCall());
  connection.on('close', common.mustCall(() => server.close()));
}));
