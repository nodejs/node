'use strict';

// Tests that `module.entrypoint` exposes the resolved URL of the entry point
// of the current thread, matching the semantics of `require.main`.

const { spawnPromisified } = require('../common');
const fixtures = require('../common/fixtures');
const assert = require('node:assert');
const { spawnSync } = require('node:child_process');
const { realpathSync } = require('node:fs');
const { test } = require('node:test');
const { pathToFileURL } = require('node:url');

function fixtureURL(...args) {
  // Unlike fixtures.fileURL(), this resolves symlinks in the path to the
  // fixtures directory: module.entrypoint is always fully resolved (unless
  // --preserve-symlinks-main is set), so the expected URL needs to be too,
  // otherwise this test would be flaky on checkouts that symlink their way
  // to the fixtures directory.
  return pathToFileURL(realpathSync(fixtures.path('module-entrypoint', ...args))).href;
}

test('CommonJS entry point', async () => {
  const entry = fixtures.path('module-entrypoint', 'main.cjs');
  const { code, stdout } = await spawnPromisified(process.execPath, [entry]);
  assert.deepStrictEqual(JSON.parse(stdout), {
    entrypoint: fixtureURL('main.cjs'),
    matchesMain: true,
  });
  assert.strictEqual(code, 0);
});

test('ESM entry point', async () => {
  const entry = fixtures.path('module-entrypoint', 'main.mjs');
  const { code, stdout } = await spawnPromisified(process.execPath, [entry]);
  assert.deepStrictEqual(JSON.parse(stdout), {
    entrypoint: fixtureURL('main.mjs'),
    matchesMain: true,
  });
  assert.strictEqual(code, 0);
});

test('extensionless entry point is resolved', async () => {
  const entry = fixtures.path('module-entrypoint', 'noext');
  const { code, stdout } = await spawnPromisified(process.execPath, [entry]);
  assert.strictEqual(stdout.trim(), fixtureURL('noext.js'));
  assert.strictEqual(code, 0);
});

test('worker threads get their own entrypoint', async () => {
  const entry = fixtures.path('module-entrypoint', 'worker-main.mjs');
  const { code, stdout } = await spawnPromisified(process.execPath, [entry]);
  const result = JSON.parse(stdout);
  assert.strictEqual(result.entrypoint, fixtureURL('worker-main.mjs'));
  assert.strictEqual(result.fileWorkerEntrypoint, fixtureURL('worker.cjs'));
  assert.strictEqual(result.evalWorkerEntrypoint, 'undefined');
  assert.match(result.dataURLWorkerEntrypoint, /^data:text\/javascript,/);
  assert.strictEqual(code, 0);
});

test('undefined for --eval', async () => {
  const { code, stdout } = await spawnPromisified(process.execPath, [
    '--eval', 'console.log(String(require("node:module").entrypoint));',
  ]);
  assert.strictEqual(stdout.trim(), 'undefined');
  assert.strictEqual(code, 0);
});

test('undefined for STDIN input', () => {
  const { status, stdout } = spawnSync(process.execPath, [], {
    input: 'console.log(String(require("node:module").entrypoint));',
    encoding: 'utf8',
  });
  assert.strictEqual(stdout.trim(), 'undefined');
  assert.strictEqual(status, 0);
});
