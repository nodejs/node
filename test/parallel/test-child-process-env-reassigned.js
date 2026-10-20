'use strict';
// When no `env` option is given, a child process inherits process.env, even
// if process.env has been replaced with another object.
// Refs: https://github.com/nodejs/node/issues/66882
const common = require('../common');
const assert = require('assert');
const {
  exec,
  execFile,
  execFileSync,
  execSync,
  fork,
  spawn,
  spawnSync,
} = require('child_process');

if (process.argv[2] === 'child') {
  process.send(process.env.TEST_REASSIGNED);
  return;
}

const printEnv = ['-p', 'process.env.TEST_REASSIGNED'];
const shellEcho = common.isWindows ?
  'echo %TEST_REASSIGNED%' :
  'echo "$TEST_REASSIGNED"';

function runSync(options) {
  const child = spawnSync(process.execPath, printEnv,
                          { encoding: 'utf8', ...options });
  assert.strictEqual(child.status, 0, child.stderr || String(child.error));
  return child.stdout.trim();
}

const originalEnv = process.env;
// Lives in the real environment, but not in the replacement object.
process.env.TEST_ONLY_IN_ORIGINAL = 'original';
process.env = { ...process.env, TEST_REASSIGNED: 'reassigned' };
delete process.env.TEST_ONLY_IN_ORIGINAL;

assert.strictEqual(runSync(), 'reassigned');
assert.strictEqual(
  execFileSync(process.execPath, printEnv, { encoding: 'utf8' }).trim(),
  'reassigned');
assert.strictEqual(execSync(shellEcho, { encoding: 'utf8' }).trim(),
                   'reassigned');

{
  const child = spawnSync(
    process.execPath,
    ['-p', 'process.env.TEST_ONLY_IN_ORIGINAL'],
    { encoding: 'utf8' });
  assert.strictEqual(child.stdout.trim(), 'undefined');
}

// The default env matches an explicit { env: process.env }.
{
  const printAll = ['-e', 'process.stdout.write(JSON.stringify(process.env))'];
  const run = (options) => JSON.parse(
    spawnSync(process.execPath, printAll, { encoding: 'utf8', ...options })
      .stdout);
  assert.deepStrictEqual(run(), run({ env: process.env }));
}

// The replacement object is used for command lookup.
if (!common.isWindows) {
  const saved = process.env;
  process.env = { ...saved, PATH: '/nonexistent' };
  const child = spawnSync('ls');
  process.env = saved;
  assert.strictEqual(child.error?.code, 'ENOENT');
}

{
  const child = spawn(process.execPath, printEnv);
  let out = '';
  child.stdout.setEncoding('utf8').on('data', (d) => { out += d; });
  let err = '';
  child.stderr.setEncoding('utf8').on('data', (d) => { err += d; });
  child.on('close', common.mustCall((code) => {
    assert.strictEqual(code, 0, err);
    assert.strictEqual(out.trim(), 'reassigned');
  }));
}

exec(shellEcho, common.mustSucceed((stdout) => {
  assert.strictEqual(stdout.trim(), 'reassigned');
}));

execFile(process.execPath, printEnv, common.mustSucceed((stdout) => {
  assert.strictEqual(stdout.trim(), 'reassigned');
}));

fork(__filename, ['child']).on('message', common.mustCall((value) => {
  assert.strictEqual(value, 'reassigned');
}));

// Putting the original object back restores inheriting the real environment.
process.env = originalEnv;
assert.strictEqual(runSync(), 'undefined');
assert.strictEqual(
  spawnSync(process.execPath, ['-p', 'process.env.TEST_ONLY_IN_ORIGINAL'],
            { encoding: 'utf8' }).stdout.trim(),
  'original');
