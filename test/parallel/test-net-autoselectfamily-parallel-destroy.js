'use strict';

const common = require('../common');
const assert = require('node:assert');
const { createConnection, createServer } = require('node:net');
const { TCP } = process.binding('tcp_wrap');

const connect = TCP.prototype.connect;
let completeFirst;
TCP.prototype.connect = function(req, address, port) {
  if (address === '127.0.0.1') {
    const oncomplete = req.oncomplete;
    req.oncomplete = (...args) => {
      completeFirst = () => oncomplete(...args);
    };
  }
  return Reflect.apply(connect, this, [req, address, port]);
};

const server = createServer((socket) => socket.end());
server.listen(0, '127.0.0.1', common.mustCall(() => {
  const port = server.address().port;
  const secondAddress = common.hasIPv6 ? '::1' : '127.0.0.2';
  const connection = createConnection({
    host: 'example.org',
    port,
    lookup: common.mustCall((host, options, callback) => {
      process.nextTick(callback, null, [
        { address: '127.0.0.1', family: 4 },
        { address: secondAddress, family: common.hasIPv6 ? 6 : 4 },
      ]);
    }),
    autoSelectFamily: true,
    autoSelectFamilyAttemptTimeout: 10,
  });

  connection.on('connectionAttempt', common.mustCallAtLeast((address) => {
    if (address === secondAddress) connection.destroy();
  }, 2));
  connection.on('connect', common.mustNotCall());
  connection.on('error', common.mustNotCall());
  connection.on('close', common.mustCall(() => {
    assert.deepStrictEqual(connection.autoSelectFamilyAttemptedAddresses,
                           [`127.0.0.1:${port}`]);
    if (completeFirst) setImmediate(completeFirst);
    server.close();
  }));
}));
