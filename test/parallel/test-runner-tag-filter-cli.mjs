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

test('not X also includes untagged tests', () => {
  const { status, stdout } = run(['--experimental-test-tag-filter=not flaky']);
  assert.strictEqual(status, 0);
  pass(stdout, 8);
  assert.match(stdout, /ok \d+ - untagged/);
  assert.doesNotMatch(stdout, /db flaky/);
  assert.doesNotMatch(stdout, /^ok \d+ - only flaky/m);
});

test('repeated --experimental-test-tag-filter ANDs together', () => {
  const { status, stdout } = run([
    '--experimental-test-tag-filter=db',
    '--experimental-test-tag-filter=not flaky',
  ]);
  assert.strictEqual(status, 0);
  pass(stdout, 2);
  assert.match(stdout, /ok \d+ - db only/);
  assert.match(stdout, /ok \d+ - db plus integration/);
  assert.doesNotMatch(stdout, /db flaky/);
});

test('bare * matches any tagged test, excludes untagged', () => {
  const { status, stdout } = run(['--experimental-test-tag-filter=*']);
  assert.strictEqual(status, 0);
  pass(stdout, 8);
  assert.doesNotMatch(stdout, /^ok \d+ - untagged/m);
});

test('wildcard prefix: db* matches db, db:postgres, etc.', () => {
  const { status, stdout } = run(['--experimental-test-tag-filter=db*']);
  assert.strictEqual(status, 0);
  pass(stdout, 4);
  assert.match(stdout, /db wildcard match/);
});

test('boolean composition: unit && !slow', () => {
  const { status, stdout } = run(['--experimental-test-tag-filter=unit && !slow']);
  assert.strictEqual(status, 0);
  pass(stdout, 1);
  assert.match(stdout, /ok \d+ - unit only/);
});

test('isolation=none produces identical filtered set', () => {
  const proc = run(['--experimental-test-tag-filter=unit && !slow']);
  const none = run(['--experimental-test-tag-filter=unit && !slow'], ['--test-isolation=none']);
  assert.strictEqual(proc.status, 0);
  assert.strictEqual(none.status, 0);
  pass(proc.stdout, 1);
  pass(none.stdout, 1);
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

test('malformed expression: unbalanced parenthesis errors at startup', () => {
  const { status, stderr, stdout } = run(['--experimental-test-tag-filter=(a']);
  assert.notStrictEqual(status, 0);
  // Match the full sentinel error line so noise elsewhere in stderr can't
  // satisfy the assertion via interleaved substrings.
  assert.match(
    stderr,
    /^TypeError \[ERR_INVALID_ARG_VALUE\]: The argument '--experimental-test-tag-filter\[0\]' expected '\)' at position \d+/m,
  );
  // No file should have been spawned.
  assert.doesNotMatch(stdout, /^ok 1 -/m);
});

test('malformed expression: dangling operator errors at startup', () => {
  const { status, stderr } = run(['--experimental-test-tag-filter=a and']);
  assert.notStrictEqual(status, 0);
  assert.match(
    stderr,
    /^TypeError \[ERR_INVALID_ARG_VALUE\]: The argument '--experimental-test-tag-filter\[0\]' unexpected token 'EOF' at position \d+/m,
  );
});

test('empty --experimental-test-tag-filter= is rejected by argv parser', () => {
  const { status, stderr } = run(['--experimental-test-tag-filter=']);
  assert.notStrictEqual(status, 0);
  assert.match(stderr, /requires an argument/);
});

test('reserved word as tag value errors at registration', () => {
  const child = spawnSync(process.execPath, [
    '-e',
    "require('node:test').test('x', { tags: ['and'] }, () => {});",
  ]);
  assert.notStrictEqual(child.status, 0);
  // Match the full sentinel error line so noise elsewhere in stderr can't
  // satisfy the assertion via interleaved substrings.
  assert.match(
    child.stderr.toString(),
    /^TypeError \[ERR_INVALID_ARG_VALUE\]: The property 'options\.tags\[0\]' must not be the reserved word/m,
  );
});
