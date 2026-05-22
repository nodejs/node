// This tests that a clean process.exit(0) from inside a probe expression
// surfaces as probe_failure, not probe_target_exit.
'use strict';

const common = require('../common');
common.skipIfInspectorDisabled();

const fixtures = require('../common/fixtures');
const { spawnSyncAndExit } = require('../common/child_process');
const { assertProbeJson } = require('../common/debugger-probe');

const cwd = fixtures.path('debugger');
const fixture = 'probe-exits-during-probe.js';
const probes = [{ expr: 'exitDuringProbe()', target: { suffix: fixture, line: 8 } }];
const location = { url: fixtures.fileURL('debugger', fixture).href, line: 8, column: 1 };

spawnSyncAndExit(process.execPath, [
  'inspect', '--json',
  '--probe', `${fixture}:8`, '--expr', probes[0].expr,
  fixture,
], { cwd }, {
  status: 1,
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
        error: { message: 'Probe evaluation did not complete' },
      }, {
        event: 'error',
        pending: [],
        error: {
          code: 'probe_failure',
          message:
            'Target process exited during probe evaluation. ' +
            'If the failure repeats, review the probe expression.',
          probe: 0,
          stderr: '',
          details: { lastCdpMethod: 'Debugger.evaluateOnCallFrame' },
        },
      }],
    });
  },
  trim: true,
});
