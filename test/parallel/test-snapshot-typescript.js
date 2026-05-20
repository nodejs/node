'use strict';

// This tests the TypeScript compiler in the snapshot.

require('../common');

const assert = require('assert');
const { spawnSync } = require('child_process');
const tmpdir = require('../common/tmpdir');
const fixtures = require('../common/fixtures');
const fs = require('fs');

tmpdir.refresh();
const blobPath = tmpdir.resolve('snapshot.blob');

// Concat test/fixtures/snapshot/typescript.js with
// test/fixtures/snapshot/typescript.js into
// tmpdir/snapshot.js.
const file = tmpdir.resolve('snapshot.js');
fs.copyFileSync(fixtures.path('snapshot', 'typescript.js'), file);
fs.appendFileSync(file,
                  fs.readFileSync(fixtures.path('snapshot', 'typescript-main.js')));

{
  const child = spawnSync(process.execPath, [
    '--snapshot-blob',
    blobPath,
    '--build-snapshot',
    file,
  ], {
    cwd: tmpdir.path
  });
  const stderr = child.stderr.toString();
  const stdout = child.stdout.toString();
  console.log(stderr);
  console.log(stdout);
  assert.strictEqual(child.status, 0);

  const stats = fs.statSync(tmpdir.resolve('snapshot.blob'));
  assert(stats.isFile());
}

{
  const outPath = tmpdir.resolve('ts-example.js');
  const child = spawnSync(process.execPath, [
    '--snapshot-blob',
    blobPath,
    fixtures.path('snapshot', 'ts-example.ts'),
    outPath,
  ], {
    cwd: tmpdir.path,
  });
  const stderr = child.stderr.toString();
  const snapshotOutput = child.stdout.toString();
  console.log(stderr);
  console.log(snapshotOutput);

  assert.strictEqual(child.status, 0);
  const result = fs.readFileSync(outPath, 'utf8');
  const expected = fs.readFileSync(
    fixtures.path('snapshot', 'ts-example.js'), 'utf8');
  assert.strictEqual(result, expected);
}
