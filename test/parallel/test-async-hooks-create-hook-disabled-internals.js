'use strict';

// Internals that use async_hooks for a purpose other than observing
// async_hooks themselves (e.g. node:test via AsyncLocalStorage) must keep
// working without --enable-async-hooks. Consumers that are themselves
// async_hooks observers (e.g. the `node.async_hooks` trace event category)
// require the flag.

const common = require('../common');
const assert = require('assert');
const { spawnSync } = require('child_process');
const test = require('node:test');

test('node:test runs without --enable-async-hooks', common.mustCall());

{
  const tmpdir = require('../common/tmpdir');
  tmpdir.refresh();

  // The `node.async_hooks` trace event category is itself an async_hooks
  // consumer, so it requires --enable-async-hooks.
  const child = spawnSync(process.execPath, [
    '--enable-async-hooks',
    '--trace-event-categories', 'node.async_hooks',
    '-e', 'setTimeout(() => {}, 1)',
  ], { cwd: tmpdir.path, encoding: 'utf8' });
  assert.strictEqual(child.stderr, '');
  assert.strictEqual(child.status, 0);

  // Without --enable-async-hooks the same category must fail.
  const childWithoutFlag = spawnSync(process.execPath, [
    '--trace-event-categories', 'node.async_hooks',
    '-e', 'setTimeout(() => {}, 1)',
  ], { cwd: tmpdir.path, encoding: 'utf8' });
  assert.notStrictEqual(childWithoutFlag.status, 0);
  assert.match(childWithoutFlag.stderr,
               /ERR_ASYNC_HOOKS_CREATE_HOOK_DISABLED/);
}
