// Tests that probes on the same line but different columns are hit separately.
'use strict';

const common = require('../common');
common.skipIfInspectorDisabled();

const fixtures = require('../common/fixtures');
const { spawnSyncAndAssert } = require('../common/child_process');
const { assertProbeJson } = require('../common/debugger-probe');

const cwd = fixtures.path('debugger');
const fixtureUrl = fixtures.fileURL('debugger', 'probe-multi-statement.js').href;

// col 3:  acc === []
// col 16: acc === [1]
// col 29: acc === [1, 2]
spawnSyncAndAssert(process.execPath, [
  'inspect',
  '--json',
  '--probe', 'probe-multi-statement.js:5:3',
  '--expr', 'acc.length',
  '--probe', 'probe-multi-statement.js:5:16',
  '--expr', 'acc.length',
  '--probe', 'probe-multi-statement.js:5:29',
  '--expr', 'acc.length',
  'probe-multi-statement.js',
], { cwd }, {
  stdout(output) {
    assertProbeJson(output, {
      v: 2,
      probes: [
        {
          expr: 'acc.length',
          target: { suffix: 'probe-multi-statement.js', line: 5, column: 3 },
        },
        {
          expr: 'acc.length',
          target: { suffix: 'probe-multi-statement.js', line: 5, column: 16 },
        },
        {
          expr: 'acc.length',
          target: { suffix: 'probe-multi-statement.js', line: 5, column: 29 },
        },
      ],
      results: [
        {
          probe: 0,
          event: 'hit',
          hit: 1,
          location: { url: fixtureUrl, line: 5, column: 3 },
          result: { type: 'number', value: 0, description: '0' },
        },
        {
          probe: 1,
          event: 'hit',
          hit: 1,
          location: { url: fixtureUrl, line: 5, column: 16 },
          result: { type: 'number', value: 1, description: '1' },
        },
        {
          probe: 2,
          event: 'hit',
          hit: 1,
          location: { url: fixtureUrl, line: 5, column: 29 },
          result: { type: 'number', value: 2, description: '2' },
        },
        { event: 'completed' },
      ],
    });
  },
  trim: true,
});
