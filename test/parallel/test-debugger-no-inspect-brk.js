// Flags: --expose-internals

// This tests that child --no-inspect and --no-inspect-brk options cannot leave
// the inspector setup disabled, while remaining valid as application args.
'use strict';

const common = require('../common');
common.skipIfInspectorDisabled();

const assert = require('assert');
const fixtures = require('../common/fixtures');
const {
  spawnSyncAndAssert,
  spawnSyncAndExit,
} = require('../common/child_process');
const { assertProbeJson } = require('../common/debugger-probe');
const { launchChildProcess } = require('internal/debugger/inspect_helpers');

const cwd = fixtures.path('debugger');
const probeUrl = fixtures.fileURL('debugger', 'probe.js').href;
const probeArgs = [
  '--probe', 'probe.js:12',
  '--expr', 'finalValue',
];
const incompatibleInspectBrk =
  /--no-inspect-brk is incompatible with node inspect before the child script/;
const incompatibleInspect =
  /--no-inspect is incompatible with node inspect before the child script/;

function assertSuccessfulProbe(childArgs) {
  spawnSyncAndAssert(process.execPath, [
    'inspect',
    '--json',
    ...probeArgs,
    '--',
    ...childArgs,
  ], { cwd }, {
    stdout(output) {
      assertProbeJson(output, {
        v: 2,
        probes: [{
          expr: 'finalValue',
          target: { suffix: 'probe.js', line: 12 },
        }],
        results: [{
          probe: 0,
          event: 'hit',
          hit: 1,
          location: { url: probeUrl, line: 12, column: 1 },
          result: { type: 'number', value: 81, description: '81' },
        }, {
          event: 'completed',
        }],
      });
    },
    trim: true,
  });
}

for (const childOptions of [
  ['--require', 'assert', '--no-inspect-brk'],
  ['--require=assert', '--no-inspect-brk'],
  ['-r', 'assert', '--no_inspect_brk'],
]) {
  spawnSyncAndExit(process.execPath, [
    'inspect',
    ...probeArgs,
    '--',
    ...childOptions,
    'probe.js',
  ], { cwd }, {
    signal: null,
    status: 1,
    stderr: incompatibleInspectBrk,
    trim: true,
  });
}

spawnSyncAndExit(process.execPath, [
  'inspect',
  ...probeArgs,
  '--',
  '--require', 'assert',
  '--no-inspect',
  'probe.js',
], { cwd }, {
  signal: null,
  status: 1,
  stderr: incompatibleInspect,
  trim: true,
});

for (const { option, error } of [
  { option: '--no-inspect-brk', error: incompatibleInspectBrk },
  { option: '--no-inspect', error: incompatibleInspect },
]) {
  spawnSyncAndExit(process.execPath, [
    'inspect',
    option,
    'probe.js',
  ], { cwd }, {
    signal: null,
    status: 1,
    stderr: error,
    trim: true,
  });

  assertSuccessfulProbe(['probe.js', option]);
}

// Node options are last-write-wins. A later --inspect-brk restores both
// startup requirements.
assertSuccessfulProbe([
  '--no-inspect',
  '--no-inspect-brk',
  '--inspect-brk',
  'probe.js',
]);

// ConfigReader rewrites these bare options to use the default path without
// consuming the following argument.
Promise.all([
  assert.rejects(
    launchChildProcess([
      '--experimental-config-file',
      '--no-inspect-brk',
      'probe.js',
    ], '127.0.0.1', 0, () => {}),
    incompatibleInspectBrk,
  ),
  assert.rejects(
    launchChildProcess([
      '--experimental-default-config-file',
      '--no-inspect-brk',
      'probe.js',
    ], '127.0.0.1', 0, () => {}),
    incompatibleInspectBrk,
  ),
  // These options imply --inspect, but do not restore --inspect-brk.
  assert.rejects(
    launchChildProcess([
      '--no-inspect',
      '--inspect-wait',
      '--no-inspect-brk',
      'probe.js',
    ], '127.0.0.1', 0, () => {}),
    incompatibleInspectBrk,
  ),
  assert.rejects(
    launchChildProcess([
      '--no-inspect',
      '--inspect-brk-node',
      '--no-inspect-brk',
      'probe.js',
    ], '127.0.0.1', 0, () => {}),
    incompatibleInspectBrk,
  ),
]).then(common.mustCall());
