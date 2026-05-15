// This tests that child Node.js flags require -- in probe mode.
'use strict';

const common = require('../common');
common.skipIfInspectorDisabled();

const fixtures = require('../common/fixtures');
const { spawnSyncAndExit } = require('../common/child_process');

const cwd = fixtures.path('debugger');

spawnSyncAndExit(process.execPath, [
  'inspect',
  '--probe', 'probe.js:12',
  '--expr', 'finalValue',
  '--inspect-port=0',
  'probe.js',
], { cwd }, {
  signal: null,
  status: 9,
  stderr: /Use -- before child Node\.js flags in probe mode/,
  trim: true,
});
