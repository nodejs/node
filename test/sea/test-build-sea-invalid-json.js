// This tests --build-sea when the config file contains invalid JSON.

'use strict';

require('../common');
const tmpdir = require('../common/tmpdir');
const { skipIfBuildSEAIsNotSupported } = require('../common/sea');
const { writeFileSync } = require('fs');
const { spawnSyncAndAssert } = require('../common/child_process');

skipIfBuildSEAIsNotSupported();

tmpdir.refresh();
const config = tmpdir.resolve('invalid.json');
writeFileSync(config, '\n{\n"main"', 'utf8');
spawnSyncAndAssert(
  process.execPath,
  ['--build-sea', config], {
    cwd: tmpdir.path,
  }, {
    status: 1,
    stderr: /INCOMPLETE_ARRAY_OR_OBJECT/,
  });
