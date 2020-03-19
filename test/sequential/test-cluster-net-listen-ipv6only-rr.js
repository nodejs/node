'use strict';

const common = require('../common');
if (!common.hasIPv6)
  common.skip('no IPv6 support');

const assert = require('assert');
const cluster = require('cluster');
const net = require('net');

// This test ensures that the `ipv6Only` option in `net.Server.listen()`
// works as expected when we use cluster with `SCHED_RR` schedulingPolicy.
cluster.schedulingPolicy = cluster.SCHED_RR;
const host = '::';
const WORKER_ACCOUNT = 3;

if (cluster.isMaster) {
  const workers = [];
  let address;

  for (let i = 0; i < WORKER_ACCOUNT; i += 1) {
    const myWorker = new Promise((resolve) => {
      const worker = cluster.fork().on('exit', common.mustCall((statusCode) => {
        assert.strictEqual(statusCode, 0);
      })).on('listening', common.mustCall((workerAddress) => {
        if (!address) {
          address = workerAddress;
        } else {
          assert.deepStrictEqual(workerAddress, address);
        }
        resolve(worker);
      }));
    });

    workers.push(myWorker);
  }

  Promise.all(workers).then(common.mustCall((resolvedWorkers) => {
    // Make sure the `ipv6Only` option works. Should be able to use the port on
    // IPv4.
    const server = net.createServer().listen({
      host: '0.0.0.0',
      port: address.port,
    }, common.mustCall(() => {
      // Exit.
      server.close();
      resolvedWorkers.forEach((resolvedWorker) => {
        resolvedWorker.disconnect();
      });
    }));
  }));
} else {
  // As the cluster member has the potential to grab any port
  // from the environment, this can cause collision when master
  // obtains the port from cluster member and tries to listen on.
  // So move this to sequential, and provide a static port.
  // Refs: https://github.com/nodejs/node/issues/25813
  net.createServer().listen({
    host: host,
    port: common.PORT,
    ipv6Only: true,
  }, common.mustCall());
}
