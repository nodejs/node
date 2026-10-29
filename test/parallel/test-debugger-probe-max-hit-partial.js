// This tests that when the target exits before a probe reaches its limit,
// the session still ends with `completed`.
'use strict';

const common = require('../common');
common.skipIfInspectorDisabled();

const fixtures = require('../common/fixtures');
const { spawnSyncAndAssert } = require('../common/child_process');
const { assertProbeJson } = require('../common/debugger-probe');

const cwd = fixtures.path('debugger');
const probeUrl = fixtures.fileURL('debugger', 'probe-max-hit.js').href;

spawnSyncAndAssert(process.execPath, [
  'inspect',
  '--json',
  '--probe', 'probe-max-hit.js:5',
  '--expr', 'index',
  '--max-hit', '10',
  'probe-max-hit.js',
], { cwd }, {
  stdout(output) {
    assertProbeJson(output, {
      v: 2,
      probes: [{
        expr: 'index',
        target: { suffix: 'probe-max-hit.js', line: 5 },
        maxHit: 10,
      }],
      results: [
        {
          probe: 0,
          event: 'hit',
          hit: 1,
          location: { url: probeUrl, line: 5, column: 3 },
          result: { type: 'number', value: 0, description: '0' },
        },
        {
          probe: 0,
          event: 'hit',
          hit: 2,
          location: { url: probeUrl, line: 5, column: 3 },
          result: { type: 'number', value: 1, description: '1' },
        },
        {
          probe: 0,
          event: 'hit',
          hit: 3,
          location: { url: probeUrl, line: 5, column: 3 },
          result: { type: 'number', value: 2, description: '2' },
        },
        { event: 'completed' },
      ],
    });
  },
  trim: true,
});
