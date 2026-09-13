'use strict';
// When no `env` option is given, a child process must inherit exactly the
// parent's current environment: same variables, same values, reflecting
// runtime additions/deletions made through process.env, and (on POSIX, where
// nothing re-sorts the block) in the same order the parent enumerates it.
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

const printEnv = ['-e', 'process.stdout.write(JSON.stringify([Object.keys(process.env), process.env]))'];

function check(output, label, expected = expectedEnv()) {
  const [childKeys, childEnv] = JSON.parse(output);
  assert.deepStrictEqual(childEnv, expected, `${label}: contents`);
  assert.strictEqual(childEnv.TEST_DEFAULT_ENV_ADDED, 'added ünïcödé ✓');
  assert.strictEqual(childEnv.TEST_DEFAULT_ENV_EMPTY, '');
  assert.strictEqual(childEnv.TEST_DEFAULT_ENV_EQUALS, 'a=b=c');
  assert.ok(!('TEST_DEFAULT_ENV_DELETED' in childEnv));
  if (!common.isWindows) {
    // Integer-like names are hoisted by object key ordering on both sides, so
    // compare the order of the remaining names.
    const nonIndex = (k) => !/^(?:0|[1-9]\d*)$/.test(k);
    assert.deepStrictEqual(childKeys.filter(nonIndex), Object.keys(expected).filter(nonIndex), `${label}: order`);
  }
}

// spawnSync, options omitted entirely.
check(spawnSync(process.execPath, printEnv, { encoding: 'utf8' }).stdout, 'spawnSync no options');
// Explicitly undefined / null env behave like the default.
check(spawnSync(process.execPath, printEnv, { encoding: 'utf8', env: undefined }).stdout, 'spawnSync env undefined');
check(spawnSync(process.execPath, printEnv, { encoding: 'utf8', env: null }).stdout, 'spawnSync env null');
// execFileSync goes through the same normalization.
check(execFileSync(process.execPath, printEnv, { encoding: 'utf8' }), 'execFileSync');
// A user-supplied env is still passed through as given (not merged).
{
  const env = { ...process.env, ONLY: 'this' };
  delete env.TEST_DEFAULT_ENV_ADDED;
  const child = spawnSync(process.execPath, printEnv, { encoding: 'utf8', env });
  assert.strictEqual(child.status, 0, child.stderr);
  const [, childEnv] = JSON.parse(child.stdout);
  assert.strictEqual(childEnv.ONLY, 'this');
  assert.ok(!('TEST_DEFAULT_ENV_ADDED' in childEnv));
}
// Async spawn (the environment is captured at spawn() time).
{
  const expectedAtSpawn = expectedEnv();
  const child = spawn(process.execPath, printEnv);
  let out = '';
  child.stdout.setEncoding('utf8').on('data', (d) => { out += d; });
  child.on('close', common.mustCall((code) => {
    assert.strictEqual(code, 0);
    check(out, 'spawn', expectedAtSpawn);
  }));
}
// A variable added after an earlier spawn is seen by a later one (no caching).
process.env.TEST_DEFAULT_ENV_LATE = 'late';
{
  const [, childEnv] = JSON.parse(spawnSync(process.execPath, printEnv, { encoding: 'utf8' }).stdout);
  assert.strictEqual(childEnv.TEST_DEFAULT_ENV_LATE, 'late');
}
