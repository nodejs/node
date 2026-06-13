// This tests that when two probes share one location but have different
// --max-hit limits, the session ends as soon as one of them reaches the limit.
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
  '--max-hit', '1',
  '--probe', 'probe-max-hit.js:5',
  '--expr', 'total',
  '--max-hit', '2',
  'probe-max-hit.js',
], { cwd }, {
  stdout(output) {
    assertProbeJson(output, {
      v: 2,
      probes: [
        { expr: 'index', target: { suffix: 'probe-max-hit.js', line: 5 }, maxHit: 1 },
        { expr: 'total', target: { suffix: 'probe-max-hit.js', line: 5 }, maxHit: 2 },
      ],
      results: [
        {
          probe: 0,
          event: 'hit',
          hit: 1,
          location: { url: probeUrl, line: 5, column: 3 },
          result: { type: 'number', value: 0, description: '0' },
        },
        {
          probe: 1,
          event: 'hit',
          hit: 1,
          location: { url: probeUrl, line: 5, column: 3 },
          result: { type: 'number', value: 0, description: '0' },
        },
        { event: 'completed' },
      ],
    });
  },
  trim: true,
});
