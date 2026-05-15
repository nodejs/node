// This tests debugger probe text output for a single hit.
'use strict';

const common = require('../common');
common.skipIfInspectorDisabled();

const fixtures = require('../common/fixtures');
const { spawnSyncAndAssert } = require('../common/child_process');
const { assertProbeText } = require('../common/debugger-probe');
const cwd = fixtures.path('debugger');
const probeUrl = fixtures.fileURL('debugger', 'probe.js').href;

spawnSyncAndAssert(process.execPath, [
  'inspect',
  '--probe', 'probe.js:12',
  '--expr', 'finalValue',
  'probe.js',
], { cwd }, {
  stdout(output) {
    assertProbeText(output,
                    `Hit 1 at ${probeUrl}:12:1\n` +
                    '  finalValue = 81\n' +
                    'Completed');
  },
  trim: true,
});
