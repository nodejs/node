'use strict';

const common = require('../common');
const { isMainThread } = require('worker_threads');

if (!isMainThread) {
  common.skip('This test only works on a main thread');
}

const assert = require('assert');
const { spawnSyncAndAssert, spawnSyncAndExit } = require('../common/child_process');

const names = [
  'PERMISSION_ENV_SECRET',
  'PERMISSION_ENV_ALLOWED',
  'PERMISSION_ENV_PREFIX_ONE',
  'PERMISSION_ENV_PREFIX_TWO',
  // Starts with NODE_, but is not one of the variables Node.js reads.
  'NODE_AUTH_TOKEN',
  // Read by Node.js itself, so it is kept.
  'TZ',
];

const env = {
  ...process.env,
  PERMISSION_ENV_SECRET: 'secret',
  PERMISSION_ENV_ALLOWED: 'allowed',
  PERMISSION_ENV_PREFIX_ONE: 'one',
  PERMISSION_ENV_PREFIX_TWO: 'two',
  NODE_AUTH_TOKEN: 'token',
  TZ: 'UTC',
};

const script = `
  const names = ${JSON.stringify(names)};
  const { environmentVariables } = process.report.getReport();
  console.log(JSON.stringify({
    values: Object.fromEntries(names.map((name) => [name, process.env[name]])),
    keys: names.filter((name) => Object.keys(process.env).includes(name)),
    report: names.filter((name) => name in environmentVariables),
    has: process.permission &&
      Object.fromEntries(names.map((name) => [name, process.permission.has('env', name)])),
    hasAll: process.permission?.has('env'),
  }));
`;

function run(...flags) {
  let result;
  spawnSyncAndAssert(
    process.execPath,
    [...flags, '--no-warnings', '-e', script],
    { env },
    {
      stdout(output) {
        result = JSON.parse(output);
      },
    });
  return result;
}

// Variables --allow-env does not grant access to are removed at startup.
{
  const result = run(
    '--permission',
    '--allow-env=PERMISSION_ENV_ALLOWED,PERMISSION_ENV_PREFIX_*');
  const visible = [
    'PERMISSION_ENV_ALLOWED',
    'PERMISSION_ENV_PREFIX_ONE',
    'PERMISSION_ENV_PREFIX_TWO',
    'TZ',
  ];
  assert.deepStrictEqual(result.values, {
    PERMISSION_ENV_ALLOWED: 'allowed',
    PERMISSION_ENV_PREFIX_ONE: 'one',
    PERMISSION_ENV_PREFIX_TWO: 'two',
    TZ: 'UTC',
  });
  assert.deepStrictEqual(result.keys, visible);
  assert.deepStrictEqual(result.report, visible);
  assert.deepStrictEqual(result.has, {
    PERMISSION_ENV_SECRET: false,
    PERMISSION_ENV_ALLOWED: true,
    PERMISSION_ENV_PREFIX_ONE: true,
    PERMISSION_ENV_PREFIX_TWO: true,
    NODE_AUTH_TOKEN: false,
    TZ: true,
  });
  assert.strictEqual(result.hasAll, false);
}

// --allow-env can be given more than once.
{
  const result = run(
    '--permission',
    '--allow-env=PERMISSION_ENV_ALLOWED',
    '--allow-env=PERMISSION_ENV_PREFIX_ONE');
  assert.deepStrictEqual(result.keys, [
    'PERMISSION_ENV_ALLOWED',
    'PERMISSION_ENV_PREFIX_ONE',
    'TZ',
  ]);
}

// Without --allow-env, only the variables Node.js reads itself are kept.
{
  const result = run('--permission');
  assert.deepStrictEqual(result.keys, ['TZ']);
  assert.strictEqual(result.hasAll, false);
}

// --allow-env=* grants access to every variable.
{
  const result = run('--permission', '--allow-env=*');
  assert.deepStrictEqual(result.keys, names);
  assert.deepStrictEqual(result.report, names);
  assert.strictEqual(result.hasAll, true);
}

// Audit mode does not remove anything.
{
  const result = run('--permission-audit', '--allow-env=PERMISSION_ENV_ALLOWED');
  assert.deepStrictEqual(result.keys, names);
  assert.strictEqual(result.has.PERMISSION_ENV_SECRET, false);
  assert.strictEqual(result.has.PERMISSION_ENV_ALLOWED, true);
  assert.strictEqual(result.hasAll, false);
}

// --allow-env requires the permission model.
spawnSyncAndExit(
  process.execPath,
  ['--allow-env=PERMISSION_ENV_ALLOWED', '-e', ''],
  { env },
  {
    status: 1,
    signal: null,
    stderr: /ERR_MISSING_OPTION.*--permission is required|--permission is required/,
  });

// Workers see the removed variables as absent, whether they share the
// environment or copy it.
spawnSyncAndAssert(
  process.execPath,
  [
    '--permission', '--allow-worker', '--no-warnings', '-e',
    `
    const assert = require('assert');
    const { Worker, SHARE_ENV } = require('worker_threads');
    for (const workerEnv of [SHARE_ENV, undefined]) {
      new Worker(
        'require("worker_threads").parentPort.postMessage(process.env.PERMISSION_ENV_SECRET)',
        { eval: true, env: workerEnv },
      ).on('message', (value) => assert.strictEqual(value, undefined));
    }
    `,
  ],
  { env },
  {});

// Environment variable names are case-insensitive on Windows.
if (common.isWindows) {
  const result = run('--permission', '--allow-env=permission_env_allowed');
  assert.deepStrictEqual(result.keys, ['PERMISSION_ENV_ALLOWED', 'TZ']);
  assert.strictEqual(result.has.PERMISSION_ENV_ALLOWED, true);
}
