// Tests that probe mode works with type stripping.
'use strict';

const common = require('../common');
common.skipIfInspectorDisabled();

const fixtures = require('../common/fixtures');
const { spawnSyncAndAssert } = require('../common/child_process');
const { assertProbeJson } = require('../common/debugger-probe');

const cwd = fixtures.path('debugger');
const tsUrl = fixtures.fileURL('debugger', 'probe-typescript.ts').href;

spawnSyncAndAssert(process.execPath, [
  'inspect',
  '--json',
  '--probe', 'probe-typescript.ts:9',
  '--expr', 'x',
  '--probe', 'probe-typescript.ts:9',
  '--expr', 'y',
  'probe-typescript.ts',
], { cwd }, {
  stdout(output) {
    assertProbeJson(output, {
      v: 2,
      probes: [
        { expr: 'x', target: { suffix: 'probe-typescript.ts', line: 9 } },
        { expr: 'y', target: { suffix: 'probe-typescript.ts', line: 9 } },
      ],
      results: [
        {
          probe: 0,
          event: 'hit',
          hit: 1,
          location: { url: tsUrl, line: 9, column: 24 },
          result: { type: 'number', value: 7, description: '7' },
        },
        {
          probe: 1,
          event: 'hit',
          hit: 1,
          location: { url: tsUrl, line: 9, column: 24 },
          result: { type: 'number', value: 8, description: '8' },
        },
        { event: 'completed' },
      ],
    });
  },
  trim: true,
});
