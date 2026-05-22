// This tests that a target script that fails to parse is surfaced as probe_target_exit.
'use strict';

const common = require('../common');
common.skipIfInspectorDisabled();

const path = require('path');
const fixtures = require('../common/fixtures');
const { spawnSyncAndExit } = require('../common/child_process');
const { assertProbeJson } = require('../common/debugger-probe');

const cwd = fixtures.path('debugger');
const fixture = 'probe-target-syntax-error.js';
const fixturePath = path.join(cwd, fixture);
const probes = [{ expr: 'x', target: { suffix: fixture, line: 3 } }];
const stderr =
  `${fixturePath}:3\n` +
  'const x = ;\n' +
  '          ^\n' +
  '\n' +
  'SyntaxError: Unexpected token \';\'\n' +
  '<stack>\n' +
  '\n' +
  'Node.js <version>';

spawnSyncAndExit(process.execPath, [
  'inspect', '--json',
  '--probe', `${fixture}:3`, '--expr', probes[0].expr,
  fixture,
], { cwd }, {
  // probe_target_exit: probing process exits 0.
  status: 0,
  signal: null,
  stdout(output) {
    assertProbeJson(output, {
      v: 2,
      probes,
      results: [{
        event: 'error',
        pending: [0],
        error: {
          code: 'probe_target_exit',
          message: `Target exited with code 1 before probes: ${fixture}:3`,
          exitCode: 1,
          stderr,
        },
      }],
    });
  },
  trim: true,
});
