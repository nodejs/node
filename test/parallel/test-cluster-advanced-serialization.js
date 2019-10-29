'use strict';
const common = require('../common');
const assert = require('assert');
const cluster = require('cluster');

if (cluster.isMaster) {
  cluster.settings.serialization = 'advanced';
  const worker = cluster.fork();
  const circular = {};
  circular.circular = circular;

  worker.on('online', common.mustCall(() => {
    worker.send(circular);

    worker.on('message', common.mustCall((msg) => {
      assert.deepStrictEqual(msg, circular);
      worker.kill();
    }));
  }));
} else {
  process.on('message', (msg) => process.send(msg));
}
