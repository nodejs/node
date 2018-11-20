'use strict';
const common = require('../common');
const assert = require('assert');
const cluster = require('cluster');
const tmpdir = require('../common/tmpdir');

if (cluster.isMaster) {
  tmpdir.refresh();

  assert.strictEqual(cluster.settings.cwd, undefined);
  cluster.fork().on('message', common.mustCall((msg) => {
    assert.strictEqual(msg, process.cwd());
  }));

  cluster.setupMaster({ cwd: tmpdir.path });
  assert.strictEqual(cluster.settings.cwd, tmpdir.path);
  cluster.fork().on('message', common.mustCall((msg) => {
    assert.strictEqual(msg, tmpdir.path);
  }));
} else {
  process.send(process.cwd());
  process.disconnect();
}
