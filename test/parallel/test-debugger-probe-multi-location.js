// Tests that when probing a suffix that resolves to two files,
// both are probed and each hit is attributed to the right script.
'use strict';

const common = require('../common');
common.skipIfInspectorDisabled();

const fixtures = require('../common/fixtures');
const { spawnSyncAndAssert } = require('../common/child_process');
const { assertProbeJson } = require('../common/debugger-probe');

const cwd = fixtures.path('debugger');
const urlA = fixtures.fileURL('debugger', 'probe-multi-a', 'utils.js').href;
const urlB = fixtures.fileURL('debugger', 'probe-multi-b', 'utils.js').href;

spawnSyncAndAssert(process.execPath, [
  'inspect',
  '--json',
  '--probe', 'utils.js:5',
  '--expr', 'b',
  'probe-multi-entry.js',
], { cwd }, {
  stdout(output) {
    assertProbeJson(output, {
      v: 2,
      probes: [
        { expr: 'b', target: { suffix: 'utils.js', line: 5 } },
      ],
      results: [
        {
          probe: 0,
          event: 'hit',
          hit: 1,
          location: { url: urlA, line: 5, column: 3 },
          result: { type: 'number', value: 2, description: '2' },
        },
        {
          probe: 0,
          event: 'hit',
          hit: 2,
          location: { url: urlB, line: 5, column: 3 },
          result: { type: 'number', value: 3, description: '3' },
        },
        { event: 'completed' },
      ],
    });
  },
  trim: true,
});
