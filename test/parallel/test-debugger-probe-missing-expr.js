// This tests that probe mode rejects a --probe without a matching --expr.
'use strict';

const common = require('../common');
common.skipIfInspectorDisabled();

const fixtures = require('../common/fixtures');
const { spawnSyncAndExit } = require('../common/child_process');

const cwd = fixtures.path('debugger');

spawnSyncAndExit(process.execPath, [
  'inspect',
  '--probe', 'probe.js:12',
  'probe.js',
], { cwd }, {
  signal: null,
  status: 9,
  stderr: /Each --probe must be followed immediately by --expr/,
  trim: true,
});
