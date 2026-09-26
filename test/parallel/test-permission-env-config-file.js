'use strict';

const common = require('../common');
const { isMainThread } = require('worker_threads');

if (!isMainThread) {
  common.skip('This test only works on a main thread');
}

const assert = require('assert');
const fs = require('fs');
const tmpdir = require('../common/tmpdir');
const { spawnSyncAndAssert } = require('../common/child_process');

tmpdir.refresh();

function writeConfig(name, allowEnv) {
  const path = tmpdir.resolve(name);
  fs.writeFileSync(path, JSON.stringify({ permission: { 'allow-env': allowEnv } }));
  return path;
}

const bothConfig = writeConfig('both.json', ['PERMISSION_ENV_A', 'PERMISSION_ENV_B']);
const allConfig = writeConfig('all.json', ['*']);
const prefixConfig = writeConfig('prefix.json', ['PERMISSION_ENV_PREFIX_DB*']);

const envFile = tmpdir.resolve('options.env');
fs.writeFileSync(envFile, 'NODE_OPTIONS="--allow-env=*"\n');

const env = {
  ...process.env,
  PERMISSION_ENV_A: 'a',
  PERMISSION_ENV_B: 'b',
  PERMISSION_ENV_PREFIX_DB_URL: 'db',
  PERMISSION_ENV_PREFIX_OTHER: 'other',
};

function visible(flags, extraEnv = {}) {
  let result;
  spawnSyncAndAssert(
    process.execPath,
    [
      ...flags, '--no-warnings', '-p',
      'JSON.stringify(Object.keys(process.env).filter((name) => name.startsWith("PERMISSION_ENV_")).sort())',
    ],
    { env: { ...env, ...extraEnv } },
    {
      stdout(output) {
        result = JSON.parse(output);
      },
    });
  return result;
}

// When the command line enables the permission model, --allow-env values from
// the configuration file can only narrow what the command line grants.
assert.deepStrictEqual(
  visible([
    '--permission',
    '--allow-env=PERMISSION_ENV_A',
    `--experimental-config-file=${bothConfig}`,
  ]),
  ['PERMISSION_ENV_A']);

assert.deepStrictEqual(
  visible(['--permission', `--experimental-config-file=${allConfig}`]),
  []);

assert.deepStrictEqual(
  visible([
    '--permission',
    '--allow-env=PERMISSION_ENV_PREFIX_*',
    `--experimental-config-file=${prefixConfig}`,
  ]),
  ['PERMISSION_ENV_PREFIX_DB_URL']);

// The same applies to NODE_OPTIONS defined in an env file.
assert.deepStrictEqual(
  visible([
    '--permission',
    '--allow-env=PERMISSION_ENV_A',
    `--env-file=${envFile}`,
  ]),
  ['PERMISSION_ENV_A']);

// When only the configuration file enables the permission model, its
// --allow-env values apply.
assert.deepStrictEqual(
  visible([`--experimental-config-file=${bothConfig}`]),
  ['PERMISSION_ENV_A', 'PERMISSION_ENV_B']);

// The NODE_OPTIONS environment variable is not a file, and can grant access.
if (!process.config.variables.node_without_node_options) {
  assert.deepStrictEqual(
    visible(
      ['--permission', '--allow-env=PERMISSION_ENV_A'],
      { NODE_OPTIONS: '--allow-env=PERMISSION_ENV_B' }),
    ['PERMISSION_ENV_A', 'PERMISSION_ENV_B']);
}
