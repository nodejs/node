// This tests that a probe expression resuming the target through its own
// inspector.Session surfaces as probe_failure.
'use strict';

const common = require('../common');
common.skipIfInspectorDisabled();

const fixtures = require('../common/fixtures');
const { spawnSyncAndExit } = require('../common/child_process');
const { assertProbeJson } = require('../common/debugger-probe');

const cwd = fixtures.path('debugger');
const fixture = 'probe-inspector-resume.js';
const probes = [{
  expr: 'callInspectorResume()',
  target: { suffix: fixture, line: 12 },
}];
const location = { url: fixtures.fileURL('debugger', fixture).href, line: 12, column: 19 };

spawnSyncAndExit(process.execPath, [
  'inspect', '--json',
  '--probe', `${fixture}:12`, '--expr', probes[0].expr,
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
        result: { type: 'number', value: 1, description: '1' },
      }, {
        event: 'error',
        pending: [],
        error: {
          code: 'probe_failure',
          message:
            'Probe session failed after a probe evaluation. If the ' +
            'failure repeats, review the most-recently-evaluated probe ' +
            'expression.',
          probe: 0,
          stderr: '',
          details: {
            lastCdpMethod: 'Debugger.resume',
            protocolError: { message: 'Can only perform operation while paused.', code: -32000 },
          },
        },
      }],
    });
  },
  trim: true,
});
