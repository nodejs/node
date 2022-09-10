'use strict';

// This tests that the warning handler is cleaned up properly
// during snapshot serialization and installed again during
// deserialization.

require('../common');

const assert = require('assert');
const { spawnSync } = require('child_process');
const tmpdir = require('../common/tmpdir');
const fixtures = require('../common/fixtures');
const path = require('path');
const fs = require('fs');

const warningScript = fixtures.path('snapshot', 'warning.js');
const blobPath = path.join(tmpdir.path, 'snapshot.blob');
const empty = fixtures.path('empty.js');

tmpdir.refresh();
{
  console.log('\n# Check snapshot scripts that do not emit warnings.');
  let child = spawnSync(process.execPath, [
    '--snapshot-blob',
    blobPath,
    '--build-snapshot',
    empty,
  ], {
    cwd: tmpdir.path
  });
  console.log('[stderr]:', child.stderr.toString());
  console.log('[stdout]:', child.stdout.toString());
  if (child.status !== 0) {
    console.log(child.signal);
    assert.strictEqual(child.status, 0);
  }
  const stats = fs.statSync(blobPath);
  assert(stats.isFile());

  child = spawnSync(process.execPath, [
    '--snapshot-blob',
    blobPath,
    warningScript,
  ], {
    cwd: tmpdir.path
  });
  console.log('[stderr]:', child.stderr.toString());
  console.log('[stdout]:', child.stdout.toString());
  if (child.status !== 0) {
    console.log(child.signal);
    assert.strictEqual(child.status, 0);
  }
  const match = child.stderr.toString().match(/Warning: test warning/g);
  assert.strictEqual(match.length, 1);
}

tmpdir.refresh();
{
  console.log('\n# Check snapshot scripts that emit ' +
              'warnings and --trace-warnings hint.');
  let child = spawnSync(process.execPath, [
    '--snapshot-blob',
    blobPath,
    '--build-snapshot',
    warningScript,
  ], {
    cwd: tmpdir.path
  });
  console.log('[stderr]:', child.stderr.toString());
  console.log('[stdout]:', child.stdout.toString());
  if (child.status !== 0) {
    console.log(child.signal);
    assert.strictEqual(child.status, 0);
  }
  const stats = fs.statSync(blobPath);
  assert(stats.isFile());
  let match = child.stderr.toString().match(/Warning: test warning/g);
  assert.strictEqual(match.length, 1);
  match = child.stderr.toString().match(/Use `node --trace-warnings/g);
  assert.strictEqual(match.length, 1);

  child = spawnSync(process.execPath, [
    '--snapshot-blob',
    blobPath,
    warningScript,
  ], {
    cwd: tmpdir.path
  });
  console.log('[stderr]:', child.stderr.toString());
  console.log('[stdout]:', child.stdout.toString());
  if (child.status !== 0) {
    console.log(child.signal);
    assert.strictEqual(child.status, 0);
  }
  // Warnings should not be handled more than once.
  match = child.stderr.toString().match(/Warning: test warning/g);
  assert.strictEqual(match.length, 1);
  match = child.stderr.toString().match(/Use `node --trace-warnings/g);
  assert.strictEqual(match.length, 1);
}

tmpdir.refresh();
{
  console.log('\n# Check --redirect-warnings');
  const warningFile1 = path.join(tmpdir.path, 'warnings.txt');
  const warningFile2 = path.join(tmpdir.path, 'warnings2.txt');

  let child = spawnSync(process.execPath, [
    '--snapshot-blob',
    blobPath,
    '--redirect-warnings',
    warningFile1,
    '--build-snapshot',
    warningScript,
  ], {
    cwd: tmpdir.path
  });
  console.log('[stderr]:', child.stderr.toString());
  console.log('[stdout]:', child.stdout.toString());
  if (child.status !== 0) {
    console.log(child.signal);
    assert.strictEqual(child.status, 0);
  }
  const stats = fs.statSync(blobPath);
  assert(stats.isFile());
  const warnings1 = fs.readFileSync(warningFile1, 'utf8');
  console.log(warningFile1, ':', warnings1);
  let match = warnings1.match(/Warning: test warning/g);
  assert.strictEqual(match.length, 1);
  match = warnings1.match(/Use `node --trace-warnings/g);
  assert.strictEqual(match.length, 1);
  assert.doesNotMatch(child.stderr.toString(), /Warning: test warning/);

  fs.rmSync(warningFile1, {
    maxRetries: 3, recursive: false, force: true
  });
  child = spawnSync(process.execPath, [
    '--snapshot-blob',
    blobPath,
    '--redirect-warnings',
    warningFile2,
    warningScript,
  ], {
    cwd: tmpdir.path
  });
  console.log('[stderr]:', child.stderr.toString());
  console.log('[stdout]:', child.stdout.toString());
  if (child.status !== 0) {
    console.log(child.signal);
    assert.strictEqual(child.status, 0);
  }
  assert(!fs.existsSync(warningFile1));

  const warnings2 = fs.readFileSync(warningFile2, 'utf8');
  console.log(warningFile2, ':', warnings1);
  match = warnings2.match(/Warning: test warning/g);
  assert.strictEqual(match.length, 1);
  match = warnings2.match(/Use `node --trace-warnings/g);
  assert.strictEqual(match.length, 1);
  assert.doesNotMatch(child.stderr.toString(), /Warning: test warning/);
}
