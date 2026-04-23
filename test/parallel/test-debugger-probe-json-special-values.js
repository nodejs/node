// This tests debugger probe JSON output for stable special-cased values.
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
  '--probe', location,
  '--expr', 'stringValue',
  '--probe', location,
  '--expr', 'booleanValue',
  '--probe', location,
  '--expr', 'undefinedValue',
  '--probe', location,
  '--expr', 'nullValue',
  '--probe', location,
  '--expr', 'nanValue',
  '--probe', location,
  '--expr', 'bigintValue',
  '--probe', location,
  '--expr', 'symbolValue',
  '--probe', location,
  '--expr', 'functionValue',
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
        { expr: 'stringValue', target: [probeTypesScript, 17] },
        { expr: 'booleanValue', target: [probeTypesScript, 17] },
        { expr: 'undefinedValue', target: [probeTypesScript, 17] },
        { expr: 'nullValue', target: [probeTypesScript, 17] },
        { expr: 'nanValue', target: [probeTypesScript, 17] },
        { expr: 'bigintValue', target: [probeTypesScript, 17] },
        { expr: 'symbolValue', target: [probeTypesScript, 17] },
        { expr: 'functionValue', target: [probeTypesScript, 17] },
        { expr: 'objectValue', target: [probeTypesScript, 17] },
        { expr: 'arrayValue', target: [probeTypesScript, 17] },
        { expr: 'errorValue', target: [probeTypesScript, 17] },
      ],
      results: [
        {
          probe: 0,
          event: 'hit',
          hit: 1,
          result: { type: 'string', value: 'hello' },
        },
        {
          probe: 1,
          event: 'hit',
          hit: 1,
          result: { type: 'boolean', value: true },
        },
        {
          probe: 2,
          event: 'hit',
          hit: 1,
          result: { type: 'undefined' },
        },
        {
          probe: 3,
          event: 'hit',
          hit: 1,
          result: { type: 'object', subtype: 'null', value: null },
        },
        {
          probe: 4,
          event: 'hit',
          hit: 1,
          result: { type: 'number', unserializableValue: 'NaN', description: 'NaN' },
        },
        {
          probe: 5,
          event: 'hit',
          hit: 1,
          result: { type: 'bigint', unserializableValue: '1n', description: '1n' },
        },
        {
          probe: 6,
          event: 'hit',
          hit: 1,
          result: { type: 'symbol', description: 'Symbol(tag)' },
        },
        {
          probe: 7,
          event: 'hit',
          hit: 1,
          result: {
            type: 'function',
            description: '() => 1',
          },
        },
        {
          probe: 8,
          event: 'hit',
          hit: 1,
          result: {
            type: 'object',
            description: 'Object',
          },
        },
        {
          probe: 9,
          event: 'hit',
          hit: 1,
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
