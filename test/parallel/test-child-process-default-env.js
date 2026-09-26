'use strict';
// When no `env` option is given, a child process must inherit exactly the
// parent's current environment: same variables and values, reflecting runtime
// additions/deletions made through process.env.
const common = require('../common');
const assert = require('assert');
const { spawn, spawnSync, execFileSync } = require('child_process');

// Mutate the environment at runtime in a few ways first.
process.env.TEST_DEFAULT_ENV_ADDED = 'added ünïcödé ✓';
process.env.TEST_DEFAULT_ENV_EMPTY = '';
process.env.TEST_DEFAULT_ENV_EQUALS = 'a=b=c';
process.env.TEST_DEFAULT_ENV_DELETED = 'x';
delete process.env.TEST_DEFAULT_ENV_DELETED;

function expectedEnv() {
  // What `{ ...process.env }` yields, minus keys whose value is undefined
  // (there are none for the real environment, but keep the definition exact).
  const copy = { ...process.env };
  for (const key of Object.keys(copy)) {
    if (copy[key] === undefined) delete copy[key];
  }
  return copy;
}

const printEnv = ['-e', 'process.stdout.write(JSON.stringify(process.env))'];

function run(options) {
  const child = spawnSync(process.execPath, printEnv, { encoding: 'utf8', ...options });
  assert.strictEqual(child.status, 0, child.stderr || String(child.error));
  return child.stdout;
}

function check(output, label, expected = expectedEnv()) {
  const childEnv = JSON.parse(output);
  assert.deepStrictEqual(childEnv, expected, `${label}: contents`);
  assert.strictEqual(childEnv.TEST_DEFAULT_ENV_ADDED, 'added ünïcödé ✓');
  assert.strictEqual(childEnv.TEST_DEFAULT_ENV_EMPTY, '');
  assert.strictEqual(childEnv.TEST_DEFAULT_ENV_EQUALS, 'a=b=c');
  assert.ok(!('TEST_DEFAULT_ENV_DELETED' in childEnv));
}

// spawnSync, options omitted entirely.
check(run(), 'spawnSync no options');
// Explicitly undefined / null env behave like the default.
check(run({ env: undefined }), 'spawnSync env undefined');
check(run({ env: null }), 'spawnSync env null');
// execFileSync goes through the same normalization.
check(execFileSync(process.execPath, printEnv, { encoding: 'utf8' }), 'execFileSync');
// A user-supplied env is still passed through as given (not merged).
{
  const env = { ...process.env, ONLY: 'this' };
  delete env.TEST_DEFAULT_ENV_ADDED;
  const childEnv = JSON.parse(run({ env }));
  assert.strictEqual(childEnv.ONLY, 'this');
  assert.ok(!('TEST_DEFAULT_ENV_ADDED' in childEnv));
}
// Async spawn (the environment is captured at spawn() time).
{
  const expectedAtSpawn = expectedEnv();
  const child = spawn(process.execPath, printEnv);
  let out = '';
  child.stdout.setEncoding('utf8').on('data', (d) => { out += d; });
  let err = '';
  child.stderr.setEncoding('utf8').on('data', (d) => { err += d; });
  child.on('close', common.mustCall((code) => {
    assert.strictEqual(code, 0, err);
    check(out, 'spawn', expectedAtSpawn);
  }));
}
// A variable added after an earlier spawn is seen by a later one (no caching).
process.env.TEST_DEFAULT_ENV_LATE = 'late';
{
  const childEnv = JSON.parse(run());
  assert.strictEqual(childEnv.TEST_DEFAULT_ENV_LATE, 'late');
}
