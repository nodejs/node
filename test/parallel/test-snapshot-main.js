'use strict';

// This tests the basic functionality of --snapshot-main.

require('../common');
const assert = require('assert');
const { spawnSync } = require('child_process');
const tmpdir = require('../common/tmpdir');
const fixtures = require('../common/fixtures');
const path = require('path');
const fs = require('fs');

tmpdir.refresh();
const file = fixtures.path('snapshot', 'mutate-fs.js');

{
  // By default, the snapshot blob path is snapshot.blob at cwd
  const child = spawnSync(process.execPath, [
    '--snapshot-main',
    file,
  ], {
    cwd: tmpdir.path
  });
  if (child.status !== 0) {
    console.log(child.stderr.toString());
    console.log(child.stdout.toString());
    assert.strictEqual(child.status, 0);
  }
  const stats = fs.statSync(path.join(tmpdir.path, 'snapshot.blob'));
  assert(stats.isFile());
}
