// This tests debugger probe text output for a single hit.
'use strict';

const common = require('../common');
common.skipIfInspectorDisabled();

const { spawnSyncAndAssert } = require('../common/child_process');
const { assertProbeText, probeScript } = require('../common/debugger-probe');

spawnSyncAndAssert(process.execPath, [
  'inspect',
  '--probe', `${probeScript}:12`,
  '--expr', 'finalValue',
  probeScript,
], {
  stdout(output) {
    assertProbeText(output,
                    `Hit 1 at ${probeScript}:12\n` +
                    '  finalValue = 81\n' +
                    'Completed');
  },
  trim: true,
});
