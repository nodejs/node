// Tests probing an indented line without specifying a column should relocate
// the breakpoint to the first executable column.
'use strict';

const common = require('../common');
common.skipIfInspectorDisabled();

const fixtures = require('../common/fixtures');
const { spawnSyncAndAssert } = require('../common/child_process');
const { assertProbeJson } = require('../common/debugger-probe');
const cwd = fixtures.path('debugger');
const fixtureUrl = fixtures.fileURL('debugger', 'probe-indented.js').href;

spawnSyncAndAssert(process.execPath, [
  'inspect',
  '--json',
  '--probe', 'probe-indented.js:6',  // No `:col`
  '--expr', 'x',
  'probe-indented.js',
], { cwd }, {
  stdout(output) {
    assertProbeJson(output, {
      v: 2,
      probes: [{
        expr: 'x',
        // No `:col` in `target`, reflecting the user spec.
        target: { suffix: 'probe-indented.js', line: 6 },
      }],
      results: [{
        probe: 0,
        event: 'hit',
        hit: 1,
        // V8 should relocate the breakpoint to the first executable column (3).
        location: { url: fixtureUrl, line: 6, column: 3 },
        // Pauses *before* the assignment runs, so `x` still holds the value from line 4.
        result: { type: 'number', value: 0, description: '0' },
      }, {
        event: 'completed',
      }],
    });
  },
  trim: true,
});
