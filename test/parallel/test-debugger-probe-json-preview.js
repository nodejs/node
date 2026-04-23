// This tests debugger probe JSON preview opt-in for object-like values.
'use strict';

const common = require('../common');
common.skipIfInspectorDisabled();

const { spawnSyncAndAssert } = require('../common/child_process');
const {
  assertProbeJson,
  probeTypesScript,
} = require('../common/debugger-probe');

const location = `${probeTypesScript}:17`;

spawnSyncAndAssert(process.execPath, [
  'inspect',
  '--json',
  '--preview',
  '--probe', location,
  '--expr', 'objectValue',
  '--probe', location,
  '--expr', 'arrayValue',
  '--probe', location,
  '--expr', 'errorValue',
  probeTypesScript,
], {
  stdout(output) {
    assertProbeJson(output, {
      v: 1,
      probes: [
        { expr: 'objectValue', target: [probeTypesScript, 17] },
        { expr: 'arrayValue', target: [probeTypesScript, 17] },
        { expr: 'errorValue', target: [probeTypesScript, 17] },
      ],
      results: [
        {
          probe: 0,
          event: 'hit',
          hit: 1,
          result: {
            type: 'object',
            description: 'Object',
            preview: {
              type: 'object',
              description: 'Object',
              overflow: false,
              properties: [
                { name: 'alpha', type: 'number', value: '1' },
                { name: 'beta', type: 'string', value: 'two' },
              ],
            },
          },
        },
        {
          probe: 1,
          event: 'hit',
          hit: 1,
          result: {
            type: 'object',
            subtype: 'array',
            description: 'Array(3)',
            preview: {
              type: 'object',
              subtype: 'array',
              description: 'Array(3)',
              overflow: false,
              properties: [
                { name: '0', type: 'number', value: '1' },
                { name: '1', type: 'string', value: 'two' },
                { name: '2', type: 'number', value: '3' },
              ],
            },
          },
        },
        {
          probe: 2,
          event: 'hit',
          hit: 1,
          result: {
            type: 'object',
            subtype: 'error',
            description: 'Error: boom',
            preview: {
              type: 'object',
              subtype: 'error',
              description: 'Error: boom',
              overflow: false,
              properties: [
                { name: 'stack', type: 'string', value: 'Error: boom' },
                { name: 'message', type: 'string', value: 'boom' },
              ],
            },
          },
        },
        { event: 'completed' },
      ],
    });
  },
  trim: true,
});
