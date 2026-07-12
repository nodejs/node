import '../common/index.mjs';
import * as fixtures from '../common/fixtures.mjs';
import assert from 'node:assert';
import { spawnSync } from 'node:child_process';
import { test } from 'node:test';

const fixture = fixtures.path('test-runner', 'tagged.js');

function run(filterArgs, extraArgs = []) {
  const args = [
    '--test',
    '--test-reporter=tap',
    ...filterArgs,
    ...extraArgs,
    fixture,
  ];
  const child = spawnSync(process.execPath, args);
  return {
    status: child.status,
    stdout: child.stdout.toString(),
    stderr: child.stderr.toString(),
  };
}

function pass(stdout, n) {
  assert.match(stdout, new RegExp(`^# pass ${n}$`, 'm'));
  assert.match(stdout, /^# fail 0$/m);
}

test('no filter: every test runs', () => {
  const { status, stdout } = run([]);
  assert.strictEqual(status, 0);
  pass(stdout, 10);
});

test('single filter: db', () => {
  const { status, stdout } = run(['--experimental-test-tag-filter=db']);
  assert.strictEqual(status, 0);
  pass(stdout, 3);
  assert.match(stdout, /ok \d+ - db only/);
  assert.match(stdout, /ok \d+ - db plus integration/);
  assert.match(stdout, /ok \d+ - db flaky/);
  assert.doesNotMatch(stdout, /unit only/);
  assert.doesNotMatch(stdout, /^ok \d+ - untagged/m);
});

test('filter is case-insensitive', () => {
  const { status, stdout } = run(['--experimental-test-tag-filter=DB']);
  assert.strictEqual(status, 0);
  pass(stdout, 3);
});

test('untagged parent runs only the matching tagged child', () => {
  // 'plain suite' has no own tags, so on its own it would be filtered out
  // by any include filter. The 'lonely child' carries the tag, so the
  // runner reinstates the parent and runs that child. The untagged sibling
  // 'plain sibling' must still be filtered out.
  const { status, stdout } = run(['--experimental-test-tag-filter=lonely']);
  assert.strictEqual(status, 0);
  pass(stdout, 1);
  assert.match(stdout, /ok \d+ - lonely child/);
  assert.doesNotMatch(stdout, /plain sibling/);
});

test('repeated --experimental-test-tag-filter ANDs together', () => {
  // db AND integration: only "db plus integration" satisfies both.
  const { status, stdout } = run([
    '--experimental-test-tag-filter=db',
    '--experimental-test-tag-filter=integration',
  ]);
  assert.strictEqual(status, 0);
  pass(stdout, 1);
  assert.match(stdout, /ok \d+ - db plus integration/);
  assert.doesNotMatch(stdout, /^ok \d+ - db only/m);
  assert.doesNotMatch(stdout, /^ok \d+ - db flaky/m);
});

test('untagged tests are excluded under any positive filter', () => {
  const { status, stdout } = run(['--experimental-test-tag-filter=db']);
  assert.strictEqual(status, 0);
  assert.doesNotMatch(stdout, /^ok \d+ - untagged/m);
});

test('isolation=none produces identical filtered set', () => {
  const proc = run(['--experimental-test-tag-filter=db']);
  const none = run(['--experimental-test-tag-filter=db'], ['--test-isolation=none']);
  assert.strictEqual(proc.status, 0);
  assert.strictEqual(none.status, 0);
  pass(proc.stdout, 3);
  pass(none.stdout, 3);
});

test('combined with --test-skip-pattern (pure AND)', () => {
  // The db filter selects 3, then skip-pattern removes any name matching /flaky/.
  const { status, stdout } = run([
    '--experimental-test-tag-filter=db',
    '--test-skip-pattern=/flaky/',
  ]);
  assert.strictEqual(status, 0);
  pass(stdout, 2);
  assert.doesNotMatch(stdout, /db flaky/);
});

test('empty --experimental-test-tag-filter= is rejected by argv parser', () => {
  const { status, stderr } = run(['--experimental-test-tag-filter=']);
  assert.notStrictEqual(status, 0);
  assert.match(stderr, /requires an argument/);
});
