'use strict';

const common = require('../common');
if (!common.hasIPv6)
  common.skip('no IPv6 support');

const assert = require('assert');
const cluster = require('cluster');
const net = require('net');

// This test ensures that dual-stack support still works for cluster module
// when `ipv6Only` is not `true`.
const host = '::';
const WORKER_COUNT = 3;

if (cluster.isPrimary) {
  const workers = [];
  let address;

  for (let i = 0; i < WORKER_COUNT; i += 1) {
    const myWorker = new Promise((resolve) => {
      const worker = cluster.fork().on('exit', common.mustCall((statusCode) => {
        assert.strictEqual(statusCode, 0);
      })).on('listening', common.mustCall((workerAddress) => {
        if (!address) {
          address = workerAddress;
        } else {
          assert.strictEqual(address.addressType, workerAddress.addressType);
          assert.strictEqual(address.host, workerAddress.host);
          assert.strictEqual(address.port, workerAddress.port);
        }
        resolve(worker);
      }));
    });

    workers.push(myWorker);
  }

  Promise.all(workers).then(common.mustCall((resolvedWorkers) => {
    const socket = net.connect({
      port: address.port,
      host: '0.0.0.0',
    }, common.mustCall(() => {
      socket.destroy();
      resolvedWorkers.forEach((resolvedWorker) => {
        resolvedWorker.disconnect();
      });
    }));
    socket.on('error', common.mustNotCall());
  }));
} else {
  net.createServer().listen({
    host,
    port: 0,
  }, common.mustCall());
}
