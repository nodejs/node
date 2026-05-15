// This tests debugger probe JSON preview opt-in for object-like values.
'use strict';

const common = require('../common');
common.skipIfInspectorDisabled();

const fixtures = require('../common/fixtures');
const { spawnSyncAndAssert } = require('../common/child_process');
const { assertProbeJson } = require('../common/debugger-probe');

const cwd = fixtures.path('debugger');
const probeArg = 'probe-types.js:17';
const target = { suffix: 'probe-types.js', line: 17 };
const location = { url: fixtures.fileURL('debugger', 'probe-types.js').href, line: 17, column: 1 };

spawnSyncAndAssert(process.execPath, [
  'inspect',
  '--json',
  '--preview',
  '--probe', probeArg,
  '--expr', 'objectValue',
  '--probe', probeArg,
  '--expr', 'arrayValue',
  '--probe', probeArg,
  '--expr', 'errorValue',
  'probe-types.js',
], { cwd }, {
  stdout(output) {
    assertProbeJson(output, {
      v: 2,
      probes: [
        { expr: 'objectValue', target },
        { expr: 'arrayValue', target },
        { expr: 'errorValue', target },
      ],
      results: [
        {
          probe: 0,
          event: 'hit',
          hit: 1,
          location,
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
          location,
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
          location,
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
