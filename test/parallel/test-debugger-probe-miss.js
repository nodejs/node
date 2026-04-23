// This tests that probe sessions report unresolved breakpoints as misses.
'use strict';

const common = require('../common');
common.skipIfInspectorDisabled();

const { spawnSyncAndAssert } = require('../common/child_process');
const { assertProbeJson, missScript } = require('../common/debugger-probe');

spawnSyncAndAssert(process.execPath, [
  'inspect',
  '--json',
  '--probe', `${missScript}:99`,
  '--expr', '42',
  missScript,
], {
  stdout(output) {
    assertProbeJson(output, {
      v: 1,
      probes: [{ expr: '42', target: [missScript, 99] }],
      results: [{
        event: 'miss',
        pending: [0],
      }],
    });
  },
  trim: true,
});
