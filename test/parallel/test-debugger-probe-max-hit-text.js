// This tests that --max-hit works with the text report.
'use strict';

const common = require('../common');
common.skipIfInspectorDisabled();

const fixtures = require('../common/fixtures');
const { spawnSyncAndAssert } = require('../common/child_process');
const { assertProbeText } = require('../common/debugger-probe');

const cwd = fixtures.path('debugger');
const probeUrl = fixtures.fileURL('debugger', 'probe-max-hit.js').href;

spawnSyncAndAssert(process.execPath, [
  'inspect',
  '--probe', 'probe-max-hit.js:5',
  '--expr', 'index',
  '--max-hit', '2',
  'probe-max-hit.js',
], { cwd }, {
  stdout(output) {
    assertProbeText(output,
                    `Hit 1 at ${probeUrl}:5:3\n` +
                    '  index = 0\n' +
                    `Hit 2 at ${probeUrl}:5:3\n` +
                    '  index = 1\n' +
                    'Completed');
  },
  trim: true,
});
