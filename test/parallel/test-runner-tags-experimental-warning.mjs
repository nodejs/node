import { expectWarning } from '../common/index.mjs';
import * as fixtures from '../common/fixtures.mjs';
import assert from 'node:assert';
import { spawnSync } from 'node:child_process';
import { it, test } from 'node:test';

const fixture = fixtures.path('test-runner', 'tagged.js');

function runChild(args, opts = {}) {
  // Strip NODE_TEST_CONTEXT so a child run() works as a top-level invocation.
  const env = { ...process.env, ...(opts.env || {}) };
  delete env.NODE_TEST_CONTEXT;
  delete env.NODE_TEST_WORKER_ID;
  const child = spawnSync(process.execPath, args, {
    stdio: ['ignore', 'pipe', 'pipe'],
    env,
  });
  return {
    status: child.status,
    stdout: child.stdout.toString(),
    stderr: child.stderr.toString(),
  };
}

function countWarnings(stderr) {
  let count = 0;
  let idx = 0;
  while (true) {
    const next = stderr.indexOf('ExperimentalWarning: Test tags', idx);
    if (next === -1) break;
    count++;
    idx = next + 1;
  }
  return count;
}

// In-process: when tags are registered with no other trigger, the warning
// fires exactly once for the lifetime of the process.
expectWarning('ExperimentalWarning', 'Test tags is an experimental feature and might change at any time');

test('the warning fires once per process for tag registration', () => {
  it('first', { tags: ['db'] }, () => {});
  it('second', { tags: ['unit'] }, () => {});
  it('third', { tags: ['integration'] }, () => {});
  // expectWarning's mustCall(_, 1) will fail if the warning is emitted a
  // second time for any of the registrations above.
});

test('--experimental-test-tag-filter fires the warning', () => {
  const { stderr } = runChild([
    '--test',
    '--test-reporter=tap',
    '--experimental-test-tag-filter=db',
    '--test-isolation=none',
    fixture,
  ]);
  assert.strictEqual(countWarnings(stderr), 1, stderr);
});

test('programmatic testTagFilters fires the warning', () => {
  const driver = `
    'use strict';
    const { run } = require('node:test');
    run({
      files: [${JSON.stringify(fixture)}],
      isolation: 'none',
      testTagFilters: ['db'],
    }).resume();
  `;
  const { stderr } = runChild(['-e', driver]);
  assert.strictEqual(countWarnings(stderr), 1, stderr);
});

test('tags: [] does not fire the warning', () => {
  const driver = `
    'use strict';
    const { it } = require('node:test');
    it('a', { tags: [] }, () => {});
    it('b', { tags: [] }, () => {});
    it('c', () => {});
  `;
  const { stderr } = runChild(['-e', driver]);
  assert.strictEqual(countWarnings(stderr), 0, stderr);
});

test('multiple triggers in one process emit the warning once', () => {
  const driver = `
    'use strict';
    const { it, run } = require('node:test');
    // Trigger 1: tag registration.
    it('a', { tags: ['db'] }, () => {});
    // Trigger 2: programmatic testTagFilters.
    run({ files: [${JSON.stringify(fixture)}], isolation: 'none', testTagFilters: ['db'] }).resume();
  `;
  const { stderr } = runChild(['-e', driver]);
  assert.strictEqual(countWarnings(stderr), 1, stderr);
});
