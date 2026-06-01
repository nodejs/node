// This tests that when a probe expression closes the inspector mid-session,
// session failure surfaces as probe_failure with downstream probes left pending.
'use strict';

const common = require('../common');
common.skipIfInspectorDisabled();

const fixtures = require('../common/fixtures');
const { spawnSyncAndExit } = require('../common/child_process');
const { assertProbeJson } = require('../common/debugger-probe');

const cwd = fixtures.path('debugger');
const fixture = 'probe-inspector-close-two-probes.js';
const marker = 'probe-inspector-close-marker';
const timeoutMs = common.platformTimeout(1000);
const probes = [
  { expr: 'closeInspector()', target: { suffix: fixture, line: 10 } },
  { expr: 'firstProbeLine', target: { suffix: fixture, line: 11 } },
];
const location = { url: fixtures.fileURL('debugger', fixture).href, line: 10, column: 24 };

spawnSyncAndExit(process.execPath, [
  'inspect', '--json', `--timeout=${timeoutMs}`,
  '--probe', `${fixture}:10`, '--expr', probes[0].expr,
  '--probe', `${fixture}:11`, '--expr', probes[1].expr,
  fixture,
], { cwd, env: { ...process.env, NODE_DEBUG: 'inspect_probe' } }, {
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
        pending: [1],
        error: {
          code: 'probe_failure',
          message:
            `Probe session timed out before probes: ${fixture}:11. ` +
            'The probe expression may be slow, hanging, or interfering ' +
            'with the inspector connection. Try increasing `--timeout`; ' +
            'if the failure persists, review the probe expressions.',
          probe: 0,
          stderr: marker,
          details: { lastCdpMethod: 'Debugger.evaluateOnCallFrame' },
        },
      }],
    });
  },
  trim: true,
});
