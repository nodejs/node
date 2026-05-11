// This tests that probe sessions report unresolved breakpoints as misses.
'use strict';

const common = require('../common');
common.skipIfInspectorDisabled();

const fixtures = require('../common/fixtures');
const { spawnSyncAndAssert } = require('../common/child_process');
const { assertProbeJson } = require('../common/debugger-probe');
const cwd = fixtures.path('debugger');

spawnSyncAndAssert(process.execPath, [
  'inspect',
  '--json',
  '--probe', 'probe-miss.js:99',
  '--expr', '42',
  'probe-miss.js',
], { cwd }, {
  stdout(output) {
    assertProbeJson(output, {
      v: 2,
      probes: [{
        expr: '42',
        target: { suffix: 'probe-miss.js', line: 99 },
      }],
      results: [{
        event: 'miss',
        pending: [0],
      }],
    });
  },
  trim: true,
});
