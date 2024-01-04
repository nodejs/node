'use strict';
require('../common');
const fixtures = require('../common/fixtures');
const assert = require('node:assert');
const { spawnSync } = require('node:child_process');
const { test } = require('node:test');
const cwd = fixtures.path('test-runner', 'default-behavior');
const env = { ...process.env, 'NODE_DEBUG': 'test_runner' };


test('lines above 100', () => {
  const args = ['--test', '--experimental-test-coverage', '--check-coverage', '--lines=101'];
  const cp = spawnSync(process.execPath, args, { cwd, env });
  assert.match(cp.stderr.toString(), /ERR_INVALID_ARG_VALUE/);
  assert.match(cp.stderr.toString(), /The argument '--lines' must be a value between 0 and 100/);
});

test('lines below 0', () => {
  const args = ['--test', '--experimental-test-coverage', '--check-coverage', '--lines=-1'];
  const cp = spawnSync(process.execPath, args, { cwd, env });
  assert.match(cp.stderr.toString(), /ERR_INVALID_ARG_VALUE/);
  assert.match(cp.stderr.toString(), /The argument '--lines' must be a value between 0 and 100/);
});


test('branches above 100', () => {
  const args = ['--test', '--experimental-test-coverage', '--check-coverage', '--branches=101'];
  const cp = spawnSync(process.execPath, args, { cwd, env });
  assert.match(cp.stderr.toString(), /ERR_INVALID_ARG_VALUE/);
  assert.match(cp.stderr.toString(), /The argument '--branches' must be a value between 0 and 100/);
});

test('branches below 0', () => {
  const args = ['--test', '--experimental-test-coverage', '--check-coverage', '--branches=-1'];
  const cp = spawnSync(process.execPath, args, { cwd, env });
  assert.match(cp.stderr.toString(), /ERR_INVALID_ARG_VALUE/);
  assert.match(cp.stderr.toString(), /The argument '--branches' must be a value between 0 and 100/);
});

test('functions above 100', () => {
  const args = ['--test', '--experimental-test-coverage', '--check-coverage', '--functions=101'];
  const cp = spawnSync(process.execPath, args, { cwd, env });
  assert.match(cp.stderr.toString(), /ERR_INVALID_ARG_VALUE/);
  assert.match(cp.stderr.toString(), /The argument '--functions' must be a value between 0 and 100/);
});

test('functions below 0', () => {
  const args = ['--test', '--experimental-test-coverage', '--check-coverage', '--functions=-1'];
  const cp = spawnSync(process.execPath, args, { cwd, env });
  assert.match(cp.stderr.toString(), /ERR_INVALID_ARG_VALUE/);
  assert.match(cp.stderr.toString(), /The argument '--functions' must be a value between 0 and 100/);
});
