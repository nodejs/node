'use strict';

require('../common');
const { spawnSyncAndExitWithoutError } = require('../common/child_process');
const fixtures = require('../common/fixtures');

const stderr = /ExperimentalWarning: Support for loading ES Module in require/;

function testPreload(preloadFlag) {
  // Test named exports.
  {
    spawnSyncAndExitWithoutError(
      process.execPath,
      [
        '--experimental-require-module',
        preloadFlag,
        fixtures.path('es-module-loaders/module-named-exports.mjs'),
        fixtures.path('printA.js'),
      ],
      {
        stdout: 'A',
        stderr,
        trim: true,
      }
    );
  }

  // Test ESM that import ESM.
  {
    spawnSyncAndExitWithoutError(
      process.execPath,
      [
        '--experimental-require-module',
        preloadFlag,
        fixtures.path('es-modules/import-esm.mjs'),
        fixtures.path('printA.js'),
      ],
      {
        stderr,
        stdout: /^world\s+A$/,
        trim: true,
      }
    );
  }

  // Test ESM that import CJS.
  {
    spawnSyncAndExitWithoutError(
      process.execPath,
      [
        '--experimental-require-module',
        preloadFlag,
        fixtures.path('es-modules/cjs-exports.mjs'),
        fixtures.path('printA.js'),
      ],
      {
        stdout: /^ok\s+A$/,
        stderr,
        trim: true,
      }
    );
  }

  // Test ESM that require() CJS.
  // Can't use the common/index.mjs here because that checks the globals, and
  // -r injects a bunch of globals.
  {
    spawnSyncAndExitWithoutError(
      process.execPath,
      [
        '--experimental-require-module',
        preloadFlag,
        fixtures.path('es-modules/require-cjs.mjs'),
        fixtures.path('printA.js'),
      ],
      {
        stdout: /^world\s+A$/,
        stderr,
        trim: true,
      }
    );
  }
}

testPreload('--require');
testPreload('--import');

// Test "type": "module" and "main" field in package.json, this is only for --require because
// --import does not support extension-less preloads.
{
  spawnSyncAndExitWithoutError(
    process.execPath,
    [
      '--experimental-require-module',
      '--require',
      fixtures.path('es-modules/package-type-module'),
      fixtures.path('printA.js'),
    ],
    {
      stdout: /^package-type-module\s+A$/,
      stderr,
      trim: true,
    }
  );
}
