'use strict';
const common = require('../common');
const assert = require('assert');
const cluster = require('cluster');

if (cluster.isMaster) {
  common.refreshTmpDir();

  assert.strictEqual(cluster.settings.cwd, undefined);
  cluster.fork().on('message', common.mustCall((msg) => {
    assert.strictEqual(msg, process.cwd());
  }));

  cluster.setupMaster({ cwd: common.tmpDir });
  assert.strictEqual(cluster.settings.cwd, common.tmpDir);
  cluster.fork().on('message', common.mustCall((msg) => {
    assert.strictEqual(msg, common.tmpDir);
  }));
} else {
  process.send(process.cwd());
  process.disconnect();
}
