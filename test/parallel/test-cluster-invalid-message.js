'use strict';
const common = require('../common');
const assert = require('assert');
const cluster = require('cluster');

if (cluster.isMaster) {
  const worker = cluster.fork();

  worker.on('exit', common.mustCall((code, signal) => {
    assert.strictEqual(code, 0);
    assert.strictEqual(signal, null);
  }));

  worker.on('online', () => {
    worker.send({
      cmd: 'NODE_CLUSTER',
      ack: -1
    }, () => {
      worker.disconnect();
    });
  });
}
