// Tests that a path-qualified suffix narrows the match to a single script.
'use strict';

const common = require('../common');
common.skipIfInspectorDisabled();

const fixtures = require('../common/fixtures');
const { spawnSyncAndAssert } = require('../common/child_process');
const { assertProbeJson } = require('../common/debugger-probe');

const cwd = fixtures.path('debugger');
const urlA = fixtures.fileURL('debugger', 'probe-multi-a', 'utils.js').href;

spawnSyncAndAssert(process.execPath, [
  'inspect',
  '--json',
  '--probe', 'probe-multi-a/utils.js:5',
  '--expr', 'b',
  'probe-multi-entry.js',
], { cwd }, {
  stdout(output) {
    assertProbeJson(output, {
      v: 2,
      probes: [
        { expr: 'b', target: { suffix: 'probe-multi-a/utils.js', line: 5 } },
      ],
      results: [
        {
          probe: 0,
          event: 'hit',
          hit: 1,
          location: { url: urlA, line: 5, column: 3 },
          result: { type: 'number', value: 2, description: '2' },
        },
        { event: 'completed' },
      ],
    });
  },
  trim: true,
});
