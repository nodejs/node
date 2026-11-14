'use strict';

const common = require('../common');
if (process.config.variables.node_without_node_options) {
  common.skip('missing NODE_OPTIONS support');
}

const assert = require('node:assert');
const { writeFileSync } = require('node:fs');
const { it } = require('node:test');
const { spawnSyncAndAssert } = require('../common/child_process');
const tmpdir = require('../common/tmpdir');

tmpdir.refresh();
writeFileSync(tmpdir.resolve('first file.env'),
              'NODE_TEST_ENV_FILE=first\nNODE_TEST_ENV_FILE_ONLY=first\nNODE_OPTIONS=--stack-trace-limit=99\n');
writeFileSync(tmpdir.resolve('second.env'), 'NODE_TEST_ENV_FILE=second\n');

const env = { ...process.env };
delete env.NODE_TEST_ENV_FILE;
delete env.NODE_TEST_ENV_FILE_ONLY;

for (const flag of ['--env-file', '--env-file-if-exists']) {
  for (const separator of ['=', ' ']) {
    it('loads ' + flag + separator + 'file from NODE_OPTIONS', () => {
      assert(process.allowedNodeEnvironmentFlags.has(flag));
      spawnSyncAndAssert(
        process.execPath,
        ['-p', 'process.env.NODE_TEST_ENV_FILE'],
        {
          cwd: tmpdir.path,
          env: { ...env, NODE_OPTIONS: flag + separator + '"first file.env"' },
        },
        { stdout: 'first\n' },
      );
    });
  }

  it('loads multiple ' + flag + ' files in order', () => {
    spawnSyncAndAssert(
      process.execPath,
      ['-p', 'process.env.NODE_TEST_ENV_FILE + ":" + process.env.NODE_TEST_ENV_FILE_ONLY'],
      {
        cwd: tmpdir.path,
        env: { ...env, NODE_OPTIONS: flag + '="first file.env" ' + flag + '=second.env' },
      },
      { stdout: 'second:first\n' },
    );
  });

  it('loads ' + flag + ' from NODE_OPTIONS before command-line files', () => {
    spawnSyncAndAssert(
      process.execPath,
      ['--env-file=second.env', '-p', 'process.env.NODE_TEST_ENV_FILE + ":" + process.env.NODE_TEST_ENV_FILE_ONLY'],
      {
        cwd: tmpdir.path,
        env: { ...env, NODE_OPTIONS: flag + '="first file.env"' },
      },
      { stdout: 'second:first\n' },
    );
  });

  it('allows access to variables loaded by ' + flag + ' with permissions', () => {
    spawnSyncAndAssert(
      process.execPath,
      ['--permission', '-p',
       'process.env.NODE_TEST_ENV_FILE + ":" + process.permission.has("env", "NODE_TEST_ENV_FILE")'],
      {
        cwd: tmpdir.path,
        env: { ...env, NODE_OPTIONS: flag + '="first file.env"' },
      },
      { stdout: 'first:true\n' },
    );
  });

  it('preserves inherited environment variables with ' + flag, () => {
    spawnSyncAndAssert(
      process.execPath,
      ['-p', 'process.env.NODE_TEST_ENV_FILE + ":" + Error.stackTraceLimit'],
      {
        cwd: tmpdir.path,
        env: {
          ...env,
          NODE_OPTIONS: flag + '="first file.env" --stack-trace-limit=42',
          NODE_TEST_ENV_FILE: 'inherited',
        },
      },
      { stdout: 'inherited:42\n' },
    );
  });

  it('requires a file argument for ' + flag, () => {
    spawnSyncAndAssert(
      process.execPath,
      ['-e', ''],
      { cwd: tmpdir.path, env: { ...env, NODE_OPTIONS: flag } },
      { status: 9, stderr: /requires an argument/ },
    );
  });

  it('handles a missing file with ' + flag, () => {
    const optional = flag === '--env-file-if-exists';
    spawnSyncAndAssert(
      process.execPath,
      ['-e', 'console.log("ok")'],
      { cwd: tmpdir.path, env: { ...env, NODE_OPTIONS: flag + '=missing.env' } },
      {
        status: optional ? 0 : 9,
        stdout: optional ? 'ok\n' : '',
        stderr: optional ? /missing\.env not found/ : /missing\.env: not found/,
      },
    );
  });
}

it('does not load files recursively from dotenv NODE_OPTIONS', () => {
  writeFileSync(tmpdir.resolve('recursive.env'),
                'NODE_TEST_ENV_FILE=outer\nNODE_OPTIONS=--env-file=recursive.env\n');
  spawnSyncAndAssert(
    process.execPath,
    ['-p', 'process.env.NODE_TEST_ENV_FILE'],
    { cwd: tmpdir.path, env: { ...env, NODE_OPTIONS: '--env-file=recursive.env' } },
    { stdout: 'outer\n' },
  );
});

it('reports malformed NODE_OPTIONS before loading files', () => {
  spawnSyncAndAssert(
    process.execPath,
    ['-e', ''],
    { cwd: tmpdir.path, env: { ...env, NODE_OPTIONS: '--env-file="first file.env' } },
    { status: 9, stderr: /invalid value for NODE_OPTIONS \(unterminated string\)/ },
  );
});
