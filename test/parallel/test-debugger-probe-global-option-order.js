// This tests that global probe options can appear after the first --probe.
'use strict';

const common = require('../common');
common.skipIfInspectorDisabled();

const fixtures = require('../common/fixtures');
const { spawnSyncAndAssert } = require('../common/child_process');
const { assertProbeJson } = require('../common/debugger-probe');

const cwd = fixtures.path('debugger');
const probeUrl = fixtures.fileURL('debugger', 'probe.js').href;

spawnSyncAndAssert(process.execPath, [
  'inspect',
  '--probe', 'probe.js:12',
  '--expr', 'finalValue',
  '--json',
  'probe.js',
], { cwd, env: { ...process.env, NODE_DEBUG: 'inspect_probe' } }, {
  stdout(output) {
    assertProbeJson(output, {
      v: 2,
      probes: [{
        expr: 'finalValue',
        target: { suffix: 'probe.js', line: 12 },
      }],
      results: [{
        probe: 0,
        event: 'hit',
        hit: 1,
        location: { url: probeUrl, line: 12, column: 1 },
        result: { type: 'number', value: 81, description: '81' },
      }, {
        event: 'completed',
      }],
    });
  },
  trim: true,
});
