// Flags: --expose-internals
'use strict';
const common = require('../common');
const { spawnSyncAndExit } = require('../common/child_process');
const { internalBinding } = require('internal/test/binding');

const { hasSmallICU } = internalBinding('config');
if (!(common.hasIntl && hasSmallICU))
  common.skip('missing Intl');

{
  spawnSyncAndExit(
    process.execPath,
    ['--icu-data-dir=/', '-e', '0'],
    {
      status: 9,
      signal: null,
      stderr: /Could not initialize ICU/
    });
}

{
  const env = { ...process.env, NODE_ICU_DATA: '/' };
  spawnSyncAndExit(
    process.execPath,
    ['-e', '0'],
    { env },
    {
      status: 9,
      signal: null,
      stderr: /Could not initialize ICU/
    });
}
