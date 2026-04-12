// This tests child --inspect-port=0 pass-through in probe mode.
'use strict';

const common = require('../common');
common.skipIfInspectorDisabled();

const assert = require('assert');
const { spawnSyncAndAssert } = require('../common/child_process');
const { probeScript } = require('../common/debugger-probe');

spawnSyncAndAssert(process.execPath, [
  'inspect',
  '--json',
  '--probe', `${probeScript}:12`,
  '--expr', 'finalValue',
  '--',
  '--inspect-port=0',
  probeScript,
], {
  stdout(output) {
    assert.deepStrictEqual(JSON.parse(output), {
      v: 1,
      probes: [{
        expr: 'finalValue',
        target: [probeScript, 12],
      }],
      results: [{
        probe: 0,
        event: 'hit',
        hit: 1,
        result: { type: 'number', value: 81, description: '81' },
      }, {
        event: 'completed',
      }],
    });
  },
  trim: true,
});
