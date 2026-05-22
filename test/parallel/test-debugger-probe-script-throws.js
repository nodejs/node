// This tests that an uncaught throw in the target between probe hits is
// surfaced as probe_target_exit, with hits before the throw preserved.
'use strict';

const common = require('../common');
common.skipIfInspectorDisabled();

const path = require('path');
const fixtures = require('../common/fixtures');
const { spawnSyncAndExit } = require('../common/child_process');
const { assertProbeJson } = require('../common/debugger-probe');

const cwd = fixtures.path('debugger');
const fixture = 'probe-script-throws.js';
const fixturePath = path.join(cwd, fixture);
const probes = [
  { expr: '\'before-throw\'', target: { suffix: fixture, line: 7 } },
  { expr: '99', target: { suffix: fixture, line: 4 } },
];
const location = { url: fixtures.fileURL('debugger', fixture).href, line: 7, column: 17 };
const stderr =
  `${fixturePath}:8\n` +
  'throw new Error(\'boom\');\n' +
  '^\n' +
  '\n' +
  'Error: boom\n' +
  '<stack>\n' +
  '\n' +
  'Node.js <version>';

spawnSyncAndExit(process.execPath, [
  'inspect', '--json',
  '--probe', `${fixture}:7`, '--expr', probes[0].expr,
  '--probe', `${fixture}:4`, '--expr', probes[1].expr,
  fixture,
], { cwd }, {
  // probe_target_exit: probing process exits 0 (hits trustworthy).
  status: 0,
  signal: null,
  stdout(output) {
    assertProbeJson(output, {
      v: 2,
      probes,
      results: [{
        probe: 0,
        event: 'hit',
        hit: 1,
        location,
        result: { type: 'string', value: 'before-throw' },
      }, {
        event: 'error',
        pending: [1],
        error: {
          code: 'probe_target_exit',
          message: `Target exited with code 1 before probes: ${fixture}:4`,
          exitCode: 1,
          stderr,
        },
      }],
    });
  },
  trim: true,
});
