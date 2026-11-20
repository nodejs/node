'use strict';

require('../common');
const assert = require('node:assert');
const fs = require('node:fs');
const path = require('node:path');
const { spawnSync } = require('node:child_process');
const tmpdir = require('../common/tmpdir');

// Env file flags are options of Node.js only when they appear before the
// script name. After it they belong to the script, which must receive them
// untouched without the file being read.

tmpdir.refresh();

const envFile = path.join(tmpdir.path, 'test.env');
fs.writeFileSync(envFile, 'FOO=fromfile\nNODE_OPTIONS=--stack-trace-limit=7\n');

const script = path.join(tmpdir.path, 'script.js');
fs.writeFileSync(script, `
console.log(JSON.stringify({
  foo: process.env.FOO ?? null,
  limit: Error.stackTraceLimit,
  args: process.argv.slice(2),
}));
`);

const env = { ...process.env };
delete env.NODE_OPTIONS;
delete env.FOO;

function run(args) {
  const child = spawnSync(process.execPath, args, { env, encoding: 'utf8' });
  assert.strictEqual(child.status, 0, child.stderr);
  return JSON.parse(child.stdout);
}

// Before the script the file is still read.
for (const flagArgs of [
  [`--env-file=${envFile}`],
  ['--env-file', envFile],
  [`--env-file-if-exists=${envFile}`],
  ['--env-file-if-exists', envFile],
]) {
  assert.deepStrictEqual(run([...flagArgs, script, 'one']),
                         { foo: 'fromfile', limit: 7, args: ['one'] });
}

// After the script the file is not read and the arguments are kept.
for (const flagArgs of [
  [`--env-file=${envFile}`],
  ['--env-file', envFile],
  [`--env-file-if-exists=${envFile}`],
  ['--env-file-if-exists', envFile],
  ['--env-file=missing.env'],
]) {
  assert.deepStrictEqual(run([script, ...flagArgs]),
                         { foo: null, limit: 10, args: flagArgs });
  assert.deepStrictEqual(run([script, 'one', ...flagArgs]),
                         { foo: null, limit: 10, args: ['one', ...flagArgs] });
}

// A file before the script is read while the same flag after it is ignored.
assert.deepStrictEqual(
  run([`--env-file=${envFile}`, script, '--env-file=missing.env']),
  { foo: 'fromfile', limit: 7, args: ['--env-file=missing.env'] });

// Nothing is read after the separator either.
assert.deepStrictEqual(
  run(['--', script, `--env-file=${envFile}`]),
  { foo: null, limit: 10, args: [`--env-file=${envFile}`] });

// A missing file before the script still stops the startup.
{
  const child = spawnSync(process.execPath,
                          ['--env-file=missing.env', script],
                          { env, encoding: 'utf8' });
  assert.strictEqual(child.status, 9);
  assert.match(child.stderr, /missing\.env: not found/);
}

// Options of Node.js between the file and the script do not hide it.
assert.deepStrictEqual(
  run([`--env-file=${envFile}`, '--no-warnings', '-r', 'node:path', script]),
  { foo: 'fromfile', limit: 7, args: [] });
