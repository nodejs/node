'use strict';

const common = require('../common');
if (!common.hasIPv6) common.skip('IPv6 loopback is unavailable');

const assert = require('node:assert');
const { createConnection, createServer } = require('node:net');
const { TCP } = process.binding('tcp_wrap');

// Hold the IPv6 completion while the IPv4 connection wins. A late completion
// for the losing handle must not emit another 'connect' or close the winner.
const connect = TCP.prototype.connect6;
let completeFirst;
let winnerSelected = false;
TCP.prototype.connect6 = function(req, address, port) {
  const oncomplete = req.oncomplete;
  req.oncomplete = (...args) => {
    const complete = () => oncomplete(...args);
    if (winnerSelected) setImmediate(complete);
    else completeFirst = complete;
  };
  return Reflect.apply(connect, this, [req, address, port]);
};

const ipv6 = createServer((socket) => socket.end());
const ipv4 = createServer(common.mustCall((socket) => socket.end()));
ipv6.listen(0, '::1', common.mustCall(() => {
  const port = ipv6.address().port;
  ipv4.listen(port, '127.0.0.1', common.mustCall(() => {
    const connection = createConnection({
      host: 'example.org',
      port,
      lookup: common.mustCall((host, options, callback) => {
        process.nextTick(callback, null, [
          { address: '::1', family: 6 },
          { address: '127.0.0.1', family: 4 },
        ]);
      }),
      autoSelectFamily: true,
      autoSelectFamilyAttemptTimeout: 10,
    });
    connection.on('connect', common.mustCall(() => {
      winnerSelected = true;
      assert.strictEqual(connection.remoteAddress, '127.0.0.1');
      assert.deepStrictEqual(connection.autoSelectFamilyAttemptedAddresses,
                             [`::1:${port}`, `127.0.0.1:${port}`]);
      if (completeFirst) setImmediate(completeFirst);
    }));
    connection.on('error', common.mustNotCall());
    connection.on('close', common.mustCall(() => {
      ipv4.close();
      ipv6.close();
    }));
  }));
}));
