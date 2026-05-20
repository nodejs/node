// This tests debugger probe JSON output for stable special-cased values.
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
  '--probe', probeArg,
  '--expr', 'stringValue',
  '--probe', probeArg,
  '--expr', 'booleanValue',
  '--probe', probeArg,
  '--expr', 'undefinedValue',
  '--probe', probeArg,
  '--expr', 'nullValue',
  '--probe', probeArg,
  '--expr', 'nanValue',
  '--probe', probeArg,
  '--expr', 'bigintValue',
  '--probe', probeArg,
  '--expr', 'symbolValue',
  '--probe', probeArg,
  '--expr', 'functionValue',
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
        { expr: 'stringValue', target },
        { expr: 'booleanValue', target },
        { expr: 'undefinedValue', target },
        { expr: 'nullValue', target },
        { expr: 'nanValue', target },
        { expr: 'bigintValue', target },
        { expr: 'symbolValue', target },
        { expr: 'functionValue', target },
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
          result: { type: 'string', value: 'hello' },
        },
        {
          probe: 1,
          event: 'hit',
          hit: 1,
          location,
          result: { type: 'boolean', value: true },
        },
        {
          probe: 2,
          event: 'hit',
          hit: 1,
          location,
          result: { type: 'undefined' },
        },
        {
          probe: 3,
          event: 'hit',
          hit: 1,
          location,
          result: { type: 'object', subtype: 'null', value: null },
        },
        {
          probe: 4,
          event: 'hit',
          hit: 1,
          location,
          result: { type: 'number', unserializableValue: 'NaN', description: 'NaN' },
        },
        {
          probe: 5,
          event: 'hit',
          hit: 1,
          location,
          result: { type: 'bigint', unserializableValue: '1n', description: '1n' },
        },
        {
          probe: 6,
          event: 'hit',
          hit: 1,
          location,
          result: { type: 'symbol', description: 'Symbol(tag)' },
        },
        {
          probe: 7,
          event: 'hit',
          hit: 1,
          location,
          result: {
            type: 'function',
            description: '() => 1',
          },
        },
        {
          probe: 8,
          event: 'hit',
          hit: 1,
          location,
          result: {
            type: 'object',
            description: 'Object',
          },
        },
        {
          probe: 9,
          event: 'hit',
          hit: 1,
          location,
          result: {
            type: 'object',
            subtype: 'array',
            description: 'Array(3)',
          },
        },
        {
          probe: 10,
          event: 'hit',
          hit: 1,
          location,
          result: {
            type: 'object',
            subtype: 'error',
            description: 'Error: boom',
          },
        },
        { event: 'completed' },
      ],
    });
  },
  trim: true,
});
