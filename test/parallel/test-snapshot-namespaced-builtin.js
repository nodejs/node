'use strict';

// This tests snapshot JS API using the example in the docs.

require('../common');
const assert = require('assert');
const { spawnSync } = require('child_process');
const tmpdir = require('../common/tmpdir');
const fs = require('fs');

tmpdir.refresh();
const blobPath = tmpdir.resolve('snapshot.blob');
{
  // The list of modules supported in the snapshot is unstable, so just check
  // a few that are known to work.
  const code = `
    require("node:v8");
    require("node:fs");
    require("node:fs/promises");
  `;
  fs.writeFileSync(
    tmpdir.resolve('entry.js'),
    code,
    'utf8'
  );
  const child = spawnSync(process.execPath, [
    '--snapshot-blob',
    blobPath,
    '--build-snapshot',
    'entry.js',
  ], {
    cwd: tmpdir.path
  });
  if (child.status !== 0) {
    console.log(child.stderr.toString());
    console.log(child.stdout.toString());
    assert.strictEqual(child.status, 0);
  }
  const stats = fs.statSync(tmpdir.resolve('snapshot.blob'));
  assert(stats.isFile());
}
