// This tests child --inspect-port=0 pass-through in probe mode.
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
  '--json',
  '--probe', 'probe.js:12',
  '--expr', 'finalValue',
  '--',
  '--inspect-port=0',
  'probe.js',
], { cwd }, {
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
