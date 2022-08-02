import { spawnPromisified } from '../common/index.mjs';
import * as fixtures from '../common/fixtures.mjs';
import tmpdir from '../common/tmpdir.js';
import assert from 'node:assert';
import path from 'node:path';
import { execPath } from 'node:process';
import { writeFile, readFile } from 'node:fs/promises';
import { describe, it } from 'node:test';

tmpdir.refresh();

function getSnapshotPath(filename) {
  const { name, dir } = path.parse(filename);
  return path.join(tmpdir.path, dir, `${name}.snapshot`);
}

async function spawnTmpfile(content = '', filename, env) {
  const header = filename.endsWith('.mjs') ?
    'import assert from \'node:assert\';' :
    'const assert = require(\'node:assert\');';
  await writeFile(path.join(tmpdir.path, filename), `${header}\n${content}`);
  const { stdout, stderr, code } = await spawnPromisified(
    execPath,
    ['--no-warnings', filename],
    { cwd: tmpdir.path, env });

  const snapshotPath = getSnapshotPath(filename);
  const snapshot = await readFile(snapshotPath, 'utf8').catch((err) => err);
  return { stdout, stderr, code, snapshot, snapshotPath, filename };
}

async function spawnFixture(filename) {
  const { dir, base, name } = path.parse(filename);
  const { stdout, stderr, code } = await spawnPromisified(execPath, ['--no-warnings', base], { cwd: dir });

  const snapshotPath = path.join(dir, `${name}.snapshot`);
  const snapshot = await readFile(snapshotPath, 'utf8').catch((err) => err);
  return { stdout, stderr, code, snapshot, snapshotPath };
}

describe('assert.snapshot', { concurrency: true }, () => {
  it('should write snapshot', async () => {
    const { stderr, code, snapshot } = await spawnFixture(fixtures.path('assert-snapshot/basic.mjs'));
    assert.strictEqual(stderr, '');
    assert.strictEqual(code, 0);
    assert.strictEqual(snapshot, '1:\n\'test\'\n#*#*#*$#*#*#*#*#*#*$#*#*#\n2:\n\'test\'');
  });

  it('should accept named snapshots', async () => {
    const { stderr, code, snapshot } = await spawnFixture(fixtures.path('assert-snapshot/named.mjs'));
    assert.strictEqual(stderr, '');
    assert.strictEqual(code, 0);
    assert.strictEqual(snapshot, 'name:\n\'test\'\n#*#*#*$#*#*#*#*#*#*$#*#*#\nanother name:\n\'test\'');
  });

  it('should succeed running multiple times', async () => {
    let result = await spawnFixture(fixtures.path('assert-snapshot/single.mjs'));
    assert.strictEqual(result.stderr, '');
    assert.strictEqual(result.code, 0);
    assert.strictEqual(result.snapshot, '1:\n\'test\'');

    result = await spawnFixture(fixtures.path('assert-snapshot/single.mjs'));
    assert.strictEqual(result.stderr, '');
    assert.strictEqual(result.code, 0);
    assert.strictEqual(result.snapshot, '1:\n\'test\'');
  });

  it('should fail when value does not match snapshot', async () => {
    const { code, stderr, snapshot } = await spawnFixture(fixtures.path('assert-snapshot/value-changed.mjs'));
    assert.match(stderr, /AssertionError \[ERR_ASSERTION\]/);
    assert.strictEqual(code, 1);
    assert.strictEqual(snapshot, '1:\n\'original\'');
  });

  it('should fail when snapshot does not contain a named snapshot', async () => {
    const { code, stderr, snapshot } = await spawnFixture(fixtures.path('assert-snapshot/non-existing-name.mjs'));
    assert.match(stderr, /AssertionError \[ERR_ASSERTION\]/);
    assert.match(stderr, /Snapshot "non existing" does not exist/);
    assert.strictEqual(code, 1);
    assert.strictEqual(snapshot, 'another name:\n\'test\'\n#*#*#*$#*#*#*#*#*#*$#*#*#\nname:\n\'test\'');
  });

  it('should fail if multiple snapshots use the same name', async () => {
    const { code, stderr, snapshot } = await spawnFixture(fixtures.path('assert-snapshot/same-name.mjs'));
    assert.match(stderr, /AssertionError \[ERR_ASSERTION\]/);
    assert.match(stderr, /Snapshot "name" already used/);
    assert.strictEqual(code, 1);
    assert.strictEqual(snapshot.code, 'ENOENT');
  });

  it('should snapshot a random replaced value', async () => {
    let result = await spawnFixture(fixtures.path('assert-snapshot/random.mjs'));
    assert.strictEqual(result.stderr, '');
    assert.strictEqual(result.code, 0);
    const originalSnapshot = result.snapshot;

    result = await spawnFixture(fixtures.path('assert-snapshot/random.mjs'));
    assert.strictEqual(result.stderr, '');
    assert.strictEqual(result.code, 0);
    assert.strictEqual(result.snapshot, originalSnapshot);
  });

  it('should serialize values', async () => {
    let result = await spawnFixture(fixtures.path('assert-snapshot/serialize.mjs'));
    assert.strictEqual(result.stderr, '');
    assert.strictEqual(result.code, 0);
    const originalSnapshot = result.snapshot;

    result = await spawnFixture(fixtures.path('assert-snapshot/serialize.mjs'));
    assert.strictEqual(result.stderr, '');
    assert.strictEqual(result.code, 0);
    assert.strictEqual(result.snapshot, originalSnapshot);
  });

  it('should override snapshot when NODE_UPDATE_SNAPSHOT=1', async () => {
    const filename = 'updated.mjs';
    await writeFile(getSnapshotPath(filename), '1:\n\'test\'');
    const { stderr, code, snapshot } = await spawnTmpfile('await assert.snapshot(\'changed\');',
                                                          filename, { NODE_UPDATE_SNAPSHOT: '1' });
    assert.strictEqual(stderr, '');
    assert.strictEqual(code, 0);
    assert.strictEqual(snapshot, '1:\n\'changed\'');
  });

  it('snapshot file should have the name of the module - esm', async () => {
    const filename = 'name.mjs';
    const { snapshotPath } = await spawnTmpfile('await assert.snapshot("test");', filename);
    assert.match(snapshotPath, /name\.snapshot$/);
  });

  it('snapshot file should have the name of the module - common js', async () => {
    const filename = 'name.js';
    const { snapshotPath } = await spawnTmpfile('assert.snapshot("test").then(() => process.exit());', filename);
    assert.match(snapshotPath, /name\.snapshot$/);
  });
});
