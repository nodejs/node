'use strict';

const common = require('../common');
const fixtures = require('../common/fixtures');
const tmpdir = require('../common/tmpdir');
const assert = require('assert');
const fs = require('fs');
const { spawnSyncAndExitWithoutError } = require('../common/child_process');

common.skipIfInspectorDisabled();

tmpdir.refresh();

// v8.takeCoverage() should be a noop if NODE_V8_COVERAGE is not set.
const intervals = 40;
{
  const { child } = spawnSyncAndExitWithoutError(process.execPath, [
    '-r',
    fixtures.path('v8-coverage', 'take-coverage'),
    fixtures.path('v8-coverage', 'interval'),
  ], {
    env: {
      ...process.env,
      NODE_DEBUG_NATIVE: 'INSPECTOR_PROFILER',
      TEST_INTERVALS: intervals
    },
  });
  console.log(child.stderr.toString());
  const coverageFiles = fs.readdirSync(tmpdir.path);
  assert.strictEqual(coverageFiles.length, 0);
}
