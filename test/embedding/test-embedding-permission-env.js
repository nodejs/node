'use strict';

// Tests node::ScrubProcessEnvironment(), and that embedders that enable the
// permission model must remove the environment variables --allow-env does not
// grant access to before node::InitializeOncePerProcess().

const common = require('../common');
const { spawnSyncAndAssert, spawnSyncAndExit } = require('../common/child_process');

const embedtest = common.resolveBuiltBinary('embedtest');

const env = {
  ...process.env,
  PERMISSION_ENV_SECRET: 'secret',
  PERMISSION_ENV_ALLOWED: 'allowed',
};

const script = 'console.log(JSON.stringify([' +
  'process.env.PERMISSION_ENV_SECRET, process.env.PERMISSION_ENV_ALLOWED]))';

const permissionFlags = [
  '--permission',
  '--allow-fs-read=*',
  '--allow-env=PERMISSION_ENV_ALLOWED',
];

// Initialization fails while the environment contains denied variables.
spawnSyncAndExit(
  embedtest,
  [...permissionFlags, script],
  {
    env: {
      PERMISSION_ENV_SECRET: 'secret',
      PERMISSION_ENV_ALLOWED: 'allowed',
    },
  },
  {
    status: 9,
    signal: null,
    stderr: /The process environment contains variables that --allow-env does not grant access to \(PERMISSION_ENV_SECRET\)\. Remove them with node::ScrubProcessEnvironment\(\) before calling node::InitializeOncePerProcess\(\)\./,
  });

// It succeeds once they have been removed.
spawnSyncAndAssert(
  embedtest,
  ['--embedder-scrub-env=PERMISSION_ENV_ALLOWED', ...permissionFlags, script],
  { env },
  {
    trim: true,
    stdout: '[null,"allowed"]',
  });

// With --allow-env=*, nothing needs to be removed.
spawnSyncAndAssert(
  embedtest,
  ['--permission', '--allow-fs-read=*', '--allow-env=*', script],
  { env },
  {
    trim: true,
    stdout: '["secret","allowed"]',
  });

// ScrubProcessEnvironment() rejects invalid patterns.
spawnSyncAndExit(
  embedtest,
  ['--embedder-scrub-env=PERMISSION_*_ENV', script],
  { env },
  {
    status: 1,
    signal: null,
    stderr: /ScrubProcessEnvironment\(\) failed/,
  });
