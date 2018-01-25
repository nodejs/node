'use strict';

// Flags: --expose-internals

const common = require('../common');

const stickyHandle = require('internal/cluster/sticky_handle');
const assert = require('assert');
const cluster = require('cluster');


if (cluster.isMaster) {
  const nbWorkers = 4;

  const handle = new stickyHandle(1, common.localhostIPv4);
  let currentWorkerId;

  for (let i = 0; i < nbWorkers; i++) {
    const worker = cluster.fork({ id: i + 1 });

    (function(worker) {
      worker.process.send = function() {
        currentWorkerId = worker.id;
      };

      handle.add(worker, function() {});
    })(worker);
  }

  const handleTCP = {
    getpeername: function(out) {
      out.address = '::ffff:192.168.1.160';
      return out;
    }
  };

  const handleTCP2 = {
    getpeername: function(out) {
      out.address = '::ffff:192.168.1.162';
      return out;
    }
  };

  const handleTCP3 = {
    getpeername: function(out) {
      return out;
    }
  };

  handle.distribute(null, handleTCP);
  const workerIdTCP1 = currentWorkerId;

  handle.distribute(null, handleTCP2);
  const workerIdTCP2 = currentWorkerId;

  handle.distribute(null, handleTCP3);
  const workerIdTCP3 = currentWorkerId;

  handle.distribute(null, handleTCP);
  assert.strictEqual(workerIdTCP1, currentWorkerId);

  handle.distribute(null, handleTCP2);
  assert.strictEqual(workerIdTCP2, currentWorkerId);

  handle.distribute(null, handleTCP3);
  assert.strictEqual(workerIdTCP3, currentWorkerId);

  assert.notStrictEqual(workerIdTCP1, workerIdTCP2);

  process.exit(0);
}
