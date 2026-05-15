// Tests that probing scripts loaded mid-session via require(esm) and import(cjs)
// still resolves and hits correctly.
'use strict';

const common = require('../common');
common.skipIfInspectorDisabled();

const fixtures = require('../common/fixtures');
const { spawnSyncAndAssert } = require('../common/child_process');
const { assertProbeJson } = require('../common/debugger-probe');

const cwd = fixtures.path('debugger');
const esmUrl = fixtures.fileURL('debugger', 'probe-late-target.mjs').href;
const cjsUrl = fixtures.fileURL('debugger', 'probe-late-target.cjs').href;

spawnSyncAndAssert(process.execPath, [
  'inspect',
  '--json',
  '--probe', 'probe-late-target.mjs:3',
  '--expr', 'value',
  '--probe', 'probe-late-target.cjs:5',
  '--expr', 'value',
  'probe-late-entry.js',
], { cwd }, {
  stdout(output) {
    assertProbeJson(output, {
      v: 2,
      probes: [
        { expr: 'value', target: { suffix: 'probe-late-target.mjs', line: 3 } },
        { expr: 'value', target: { suffix: 'probe-late-target.cjs', line: 5 } },
      ],
      results: [
        {
          probe: 1,
          event: 'hit',
          hit: 1,
          location: { url: cjsUrl, line: 5, column: 3 },
          result: { type: 'number', value: 3, description: '3' },
        },
        {
          probe: 0,
          event: 'hit',
          hit: 1,
          location: { url: esmUrl, line: 3, column: 3 },
          result: { type: 'number', value: 5, description: '5' },
        },
        { event: 'completed' },
      ],
    });
  },
  trim: true,
});
