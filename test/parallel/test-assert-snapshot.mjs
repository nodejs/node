import { spawnPromisified } from '../common/index.mjs';
import * as fixtures from '../common/fixtures.mjs';
import tmpdir from '../common/tmpdir.js';
import assert from 'node:assert';
import path from 'node:path';
import { execPath } from 'node:process';
import { writeFile, readFile, unlink } from 'node:fs/promises';
import { describe, it } from 'node:test';

tmpdir.refresh();

function getSnapshotPath(filename) {
  const { name, dir } = path.parse(filename);
  return path.join(tmpdir.path, dir, `${name}.snapshot`);
}

async function spawnTmpfile(content = '', filename, extraFlags = []) {
  const header = filename.endsWith('.mjs') ?
    'import assert from \'node:assert\';' :
    'const assert = require(\'node:assert\');';
  await writeFile(path.join(tmpdir.path, filename), `${header}\n${content}`);
  const { stdout, stderr, code } = await spawnPromisified(
    execPath,
    ['--no-warnings', ...extraFlags, filename],
    { cwd: tmpdir.path });

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
    const { stderr, code, snapshot, snapshotPath } = await spawnFixture(fixtures.path('assert-snapshot/basic.mjs'));
    await unlink(snapshotPath);
    assert.strictEqual(stderr, '');
    assert.strictEqual(code, 0);
    assert.match(snapshot, /^name:\r?\n'test'$/);
  });

  it('should write multiple snapshots', async () => {
    const { stderr, code, snapshot, snapshotPath } = await spawnFixture(fixtures.path('assert-snapshot/multiple.mjs'));
    await unlink(snapshotPath);
    assert.strictEqual(stderr, '');
    assert.strictEqual(code, 0);
    assert.match(snapshot, /^name:\n'test'\r?\n#\*#\*#\*#\*#\*#\*#\*#\*#\*#\*#\*#\r?\nanother name:\r?\n'test'$/);
  });

  it('should succeed running multiple times', async () => {
    let result = await spawnFixture(fixtures.path('assert-snapshot/single.mjs'), false);
    assert.strictEqual(result.stderr, '');
    assert.strictEqual(result.code, 0);
    assert.match(result.snapshot, /^snapshot:\r?\n'test'$/);

    result = await spawnFixture(fixtures.path('assert-snapshot/single.mjs'));
    await unlink(result.snapshotPath);
    assert.strictEqual(result.stderr, '');
    assert.strictEqual(result.code, 0);
    assert.match(result.snapshot, /^snapshot:\r?\n'test'$/);
  });

  it('should fail when name is not provided', async () => {
    for (const name of [1, undefined, null, {}, function() {}]) {
      await assert.rejects(() => assert.snapshot('', name), {
        code: 'ERR_INVALID_ARG_TYPE',
        name: 'TypeError',
        message: /^The "name" argument must be of type string/
      });
    }
  });

  it('should fail when value does not match snapshot', async () => {
    const { code, stderr, snapshot } = await spawnFixture(fixtures.path('assert-snapshot/value-changed.mjs'));
    assert.match(stderr, /AssertionError \[ERR_ASSERTION\]/);
    assert.strictEqual(code, 1);
    assert.match(snapshot, /^snapshot:\r?\n'original'$/);
  });

  it('should fail when snapshot does not contain a named snapshot', async () => {
    const { code, stderr, snapshot } = await spawnFixture(fixtures.path('assert-snapshot/non-existing-name.mjs'));
    assert.match(stderr, /AssertionError \[ERR_ASSERTION\]/);
    assert.match(stderr, /Snapshot "non existing" does not exist/);
    assert.strictEqual(code, 1);
    assert.match(snapshot, /^another name:\r?\n'test'\r?\n#\*#\*#\*#\*#\*#\*#\*#\*#\*#\*#\*#\r?\nname:\r?\n'test'$/);
  });

  it('should snapshot a random replaced value', async () => {
    const originalSnapshot = await readFile(fixtures.path('assert-snapshot/random.snapshot'), 'utf8');
    const { stderr, code, snapshot } = await spawnFixture(fixtures.path('assert-snapshot/random.mjs'));
    assert.strictEqual(stderr, '');
    assert.strictEqual(code, 0);
    assert.strictEqual(snapshot, originalSnapshot);
  });

  it('should serialize values', async () => {
    const originalSnapshot = await readFile(fixtures.path('assert-snapshot/serialize.snapshot'), 'utf8');
    const { stderr, code, snapshot } = await spawnFixture(fixtures.path('assert-snapshot/serialize.mjs'));
    assert.strictEqual(stderr, '');
    assert.strictEqual(code, 0);
    assert.strictEqual(snapshot, originalSnapshot);
  });

  it('should override snapshot when passing --update-assert-snapshot', async () => {
    const filename = 'updated.mjs';
    await writeFile(getSnapshotPath(filename), 'snapshot:\n\'test\'');
    const { stderr, code, snapshot } = await spawnTmpfile('await assert.snapshot(\'changed\', \'snapshot\');',
                                                          filename, ['--update-assert-snapshot']);
    assert.strictEqual(stderr, '');
    assert.strictEqual(code, 0);
    assert.match(snapshot, /^snapshot:\r?\n'changed'$/);
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
