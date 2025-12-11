'use strict';
const common = require('../common');
const assert = require('assert');
const cluster = require('cluster');
const { ChildProcess } = require('child_process');
const dc = require('diagnostics_channel');

if (cluster.isPrimary) {
  dc.subscribe('child_process', common.mustCall(({ process }) => {
    assert.strictEqual(process instanceof ChildProcess, true);
  }));
  const worker = cluster.fork();
  worker.on('online', common.mustCall(() => {
    worker.send('disconnect');
  }));
} else {
  process.on('message', common.mustCall((msg) => {
    assert.strictEqual(msg, 'disconnect');
    process.disconnect();
  }));
}
