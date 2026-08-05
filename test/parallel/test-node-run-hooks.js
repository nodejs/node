'use strict';

const common = require('../common');
common.requireNoPackageJSONAbove();

const { it, describe } = require('node:test');
const assert = require('node:assert');

const fixtures = require('../common/fixtures');
const hooksDir = fixtures.path('run-script/hooks');

describe('node --run [command] with lifecycle hooks', () => {
  it('runs the "pre<script>" hook before the script with --enable-run-hooks', async () => {
    const child = await common.spawnPromisified(
      process.execPath,
      ['--run', 'build', '--enable-run-hooks'],
      { cwd: hooksDir },
    );
    assert.strictEqual(child.code, 0);
    assert.strictEqual(child.stderr, '');
    assert.match(child.stdout, /prebuild-output/);
    assert.match(child.stdout, /build-output/);
    // The hook must run strictly before the main script.
    assert.ok(
      child.stdout.indexOf('prebuild-output') <
        child.stdout.indexOf('build-output'),
      `expected prebuild-output before build-output, got: ${child.stdout}`,
    );
  });

  it('does not run the "pre<script>" hook without the flag', async () => {
    const child = await common.spawnPromisified(
      process.execPath,
      ['--run', 'build'],
      { cwd: hooksDir },
    );
    assert.strictEqual(child.code, 0);
    assert.strictEqual(child.stderr, '');
    assert.match(child.stdout, /build-output/);
    assert.doesNotMatch(child.stdout, /prebuild-output/);
  });

  it('aborts and does not run the script when the hook fails', async () => {
    const child = await common.spawnPromisified(
      process.execPath,
      ['--run', 'fail-build', '--enable-run-hooks'],
      { cwd: hooksDir },
    );
    // The failing hook exits with status 1 and the main script never runs.
    assert.strictEqual(child.code, 1);
    assert.match(child.stdout, /prefail-output/);
    assert.doesNotMatch(child.stdout, /fail-build-main-output/);
  });

  it('runs the failing script normally when hooks are disabled', async () => {
    const child = await common.spawnPromisified(
      process.execPath,
      ['--run', 'fail-build'],
      { cwd: hooksDir },
    );
    // Without the flag the "prefail-build" hook is ignored entirely.
    assert.strictEqual(child.code, 0);
    assert.match(child.stdout, /fail-build-main-output/);
    assert.doesNotMatch(child.stdout, /prefail-output/);
  });

  it('passes positional arguments to the script only, not the hook', async () => {
    const child = await common.spawnPromisified(
      process.execPath,
      ['--run', 'with-args', '--enable-run-hooks', '--', 'EXTRA1', 'EXTRA2'],
      { cwd: hooksDir },
    );
    assert.strictEqual(child.code, 0);
    assert.strictEqual(child.stderr, '');
    assert.match(child.stdout, /prehook-no-args/);
    assert.match(child.stdout, /main-output EXTRA1 EXTRA2/);
    // The hook must not receive the positional arguments.
    assert.doesNotMatch(child.stdout, /prehook-no-args EXTRA1/);
  });

  it('runs the script when there is no matching hook', async () => {
    const child = await common.spawnPromisified(
      process.execPath,
      ['--run', 'no-hook', '--enable-run-hooks'],
      { cwd: hooksDir },
    );
    assert.strictEqual(child.code, 0);
    assert.strictEqual(child.stderr, '');
    assert.match(child.stdout, /no-hook-main-output/);
  });

  it('ignores a hook whose value is not a string', async () => {
    const child = await common.spawnPromisified(
      process.execPath,
      ['--run', 'bad-type', '--enable-run-hooks'],
      { cwd: hooksDir },
    );
    assert.strictEqual(child.code, 0);
    assert.strictEqual(child.stderr, '');
    assert.match(child.stdout, /bad-type-main-output/);
  });
});
