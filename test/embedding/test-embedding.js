'use strict';
const common = require('../common');
const fixtures = require('../common/fixtures');
const tmpdir = require('../common/tmpdir');
const assert = require('assert');
const child_process = require('child_process');
const path = require('path');
const fs = require('fs');

tmpdir.refresh();
common.allowGlobals(global.require);
common.allowGlobals(global.embedVars);

function resolveBuiltBinary(bin) {
  let binary = `out/${common.buildType}/${bin}`;
  if (common.isWindows) {
    binary += '.exe';
  }
  return path.resolve(__dirname, '..', '..', binary);
}

const binary = resolveBuiltBinary('embedtest');

assert.strictEqual(
  child_process.spawnSync(binary, ['console.log(42)'])
    .stdout.toString().trim(),
  '42');

assert.strictEqual(
  child_process.spawnSync(binary, ['console.log(embedVars.nön_ascıı)'])
    .stdout.toString().trim(),
  '🏳️‍🌈');

assert.strictEqual(
  child_process.spawnSync(binary, ['console.log(42)'])
    .stdout.toString().trim(),
  '42');

assert.strictEqual(
  child_process.spawnSync(binary, ['throw new Error()']).status,
  1);

assert.strictEqual(
  child_process.spawnSync(binary, ['process.exitCode = 8']).status,
  8);


const fixturePath = JSON.stringify(fixtures.path('exit.js'));
assert.strictEqual(
  child_process.spawnSync(binary, [`require(${fixturePath})`, 92]).status,
  92);

function getReadFileCodeForPath(path) {
  return `(require("fs").readFileSync(${JSON.stringify(path)}, "utf8"))`;
}

// Basic snapshot support
for (const extraSnapshotArgs of [[], ['--embedder-snapshot-as-file']]) {
  // readSync + eval since snapshots don't support userland require() (yet)
  const snapshotFixture = fixtures.path('snapshot', 'echo-args.js');
  const blobPath = path.join(tmpdir.path, 'embedder-snapshot.blob');
  const buildSnapshotArgs = [
    `eval(${getReadFileCodeForPath(snapshotFixture)})`, 'arg1', 'arg2',
    '--embedder-snapshot-blob', blobPath, '--embedder-snapshot-create',
    ...extraSnapshotArgs,
  ];
  const runEmbeddedArgs = [
    '--embedder-snapshot-blob', blobPath, ...extraSnapshotArgs, 'arg3', 'arg4',
  ];

  fs.rmSync(blobPath, { force: true });
  assert.strictEqual(child_process.spawnSync(binary, [
    '--', ...buildSnapshotArgs,
  ], {
    cwd: tmpdir.path,
  }).status, 0);
  const spawnResult = child_process.spawnSync(binary, ['--', ...runEmbeddedArgs]);
  assert.deepStrictEqual(JSON.parse(spawnResult.stdout), {
    originalArgv: [binary, ...buildSnapshotArgs],
    currentArgv: [binary, ...runEmbeddedArgs],
  });
}

// Create workers and vm contexts after deserialization
{
  const snapshotFixture = fixtures.path('snapshot', 'create-worker-and-vm.js');
  const blobPath = path.join(tmpdir.path, 'embedder-snapshot.blob');
  const buildSnapshotArgs = [
    `eval(${getReadFileCodeForPath(snapshotFixture)})`,
    '--embedder-snapshot-blob', blobPath, '--embedder-snapshot-create',
  ];

  fs.rmSync(blobPath, { force: true });
  assert.strictEqual(child_process.spawnSync(binary, [
    '--', ...buildSnapshotArgs,
  ], {
    cwd: tmpdir.path,
  }).status, 0);
  assert.strictEqual(
    child_process.spawnSync(binary, ['--', '--embedder-snapshot-blob', blobPath]).status,
    0);
}
