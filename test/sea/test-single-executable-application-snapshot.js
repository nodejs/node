'use strict';

require('../common');

const {
  buildSEA,
  skipIfBuildSEAIsNotSupported,
} = require('../common/sea');

skipIfBuildSEAIsNotSupported();

// This tests the snapshot support in single executable applications.

const tmpdir = require('../common/tmpdir');
const { writeFileSync } = require('fs');
const {
  spawnSyncAndAssert,
  spawnSyncAndExit,
} = require('../common/child_process');
const fixtures = require('../common/fixtures');
const assert = require('assert');

{
  tmpdir.refresh();

  writeFileSync(tmpdir.resolve('snapshot.js'), '', 'utf-8');
  writeFileSync(tmpdir.resolve('sea-config.json'), `
  {
    "main": "snapshot.js",
    "output": "sea-prep.blob",
    "useSnapshot": true
  }
  `);

  spawnSyncAndExit(
    process.execPath,
    ['--experimental-sea-config', 'sea-config.json'],
    {
      cwd: tmpdir.path,
    },
    {
      status: 1,
      signal: null,
      stderr: /snapshot\.js does not invoke v8\.startupSnapshot\.setDeserializeMainFunction\(\)/,
    });
}

{
  tmpdir.refresh();

  const outputFile = buildSEA(fixtures.path('sea', 'snapshot'));

  spawnSyncAndAssert(
    outputFile,
    {
      env: {
        NODE_DEBUG_NATIVE: 'SEA,MKSNAPSHOT',
        ...process.env,
      },
    },
    {
      trim: true,
      stdout: 'Hello from snapshot',
      stderr(output) {
        assert.doesNotMatch(
          output,
          /Single executable application is an experimental feature/);
      },
    },
  );
}
