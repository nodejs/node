'use strict';

// Node.js internals that rely on async hooks must keep working without
// --allow-async-hooks.

const common = require('../common');
const assert = require('assert');
const { spawnSync } = require('child_process');
const test = require('node:test');

test('node:test runs without --allow-async-hooks', common.mustCall());

{
  const tmpdir = require('../common/tmpdir');
  tmpdir.refresh();
  const child = spawnSync(process.execPath, [
    '--trace-event-categories', 'node.async_hooks',
    '-e', 'setTimeout(() => {}, 1)',
  ], { cwd: tmpdir.path, encoding: 'utf8' });
  assert.strictEqual(child.stderr, '');
  assert.strictEqual(child.status, 0);
}
