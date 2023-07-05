'use strict';

// This tests snapshot JS API using the example in the docs.

require('../common');
const assert = require('assert');
const { spawnSync } = require('child_process');
const tmpdir = require('../common/tmpdir');
const path = require('path');
const fs = require('fs');

tmpdir.refresh();
const blobPath = path.join(tmpdir.path, 'snapshot.blob');
const code = `
require('v8').startupSnapshot.setDeserializeMainFunction(() => {
  console.log(JSON.stringify(process.argv));
});
`;
{
  fs.writeFileSync(path.join(tmpdir.path, 'entry.js'), code, 'utf8');
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
  const stats = fs.statSync(path.join(tmpdir.path, 'snapshot.blob'));
  assert(stats.isFile());
}

{
  const child = spawnSync(process.execPath, [
    '--snapshot-blob',
    blobPath,
    'argv1',
    'argv2',
  ], {
    cwd: tmpdir.path,
    env: {
      ...process.env,
    }
  });

  const stdout = JSON.parse(child.stdout.toString().trim());
  assert.deepStrictEqual(stdout, [
    process.execPath,
    'argv1',
    'argv2',
  ]);
  assert.strictEqual(child.status, 0);
}
