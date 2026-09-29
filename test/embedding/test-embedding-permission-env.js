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
// The message lists at most five of them, so the child gets a minimal
// environment. It still needs the dynamic loader's search path, e.g. when
// built against a shared OpenSSL that lives outside the default search path.
// The child's environment is not fully under the test's control either: macOS
// adds __CF_USER_TEXT_ENCODING to it, so the message may list more than the
// variables set here.
const minimalEnv = {
  PERMISSION_ENV_SECRET: 'secret',
  PERMISSION_ENV_ALLOWED: 'allowed',
};
let loaderPathVar = 'LD_LIBRARY_PATH';
if (common.isWindows) loaderPathVar = 'PATH';
else if (common.isMacOS) loaderPathVar = 'DYLD_LIBRARY_PATH';
else if (common.isAIX) loaderPathVar = 'LIBPATH';
if (process.env[loaderPathVar] !== undefined) {
  minimalEnv[loaderPathVar] = process.env[loaderPathVar];
}
spawnSyncAndExit(
  embedtest,
  [...permissionFlags, script],
  { env: minimalEnv },
  {
    status: 9,
    signal: null,
    stderr: /The process environment contains variables that --allow-env does not grant access to \([^)]*\bPERMISSION_ENV_SECRET\b[^)]*\)\. Remove them with node::ScrubProcessEnvironment\(\) before calling node::InitializeOncePerProcess\(\)\./,
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
