// This tests that a limited probe that is never reached is still reported as a missed probe.
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
  '--max-hit', '3',
  'probe-miss.js',
], { cwd }, {
  stdout(output) {
    assertProbeJson(output, {
      v: 2,
      probes: [{
        expr: '42',
        target: { suffix: 'probe-miss.js', line: 99 },
        maxHit: 3,
      }],
      results: [{
        event: 'miss',
        pending: [0],
      }],
    });
  },
  trim: true,
});
