// This tests that a probe expression resuming the target through its own
// inspector.Session is surfaced as a probe-side failure. The terminal event
// can be either probe_failure or probe_timeout depending on a race in V8's
// nested pause-loop drain.
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
], { cwd, env: { ...process.env, NODE_DEBUG: 'inspect_probe' } }, {
  status: 1,
  signal: null,
  stdout(output) {
    const expected = {
      v: 2,
      probes,
      results: [{
        probe: 0,
        event: 'hit',
        hit: 1,
        location,
        result: { type: 'number', value: 1, description: '1' },
      }]
    };

    const actual = JSON.parse(output);

    const code = actual.results.at(-1)?.error?.code;
    if (code === 'probe_failure') {
      expected.results.push({
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
      });
    } else if (code === 'probe_timeout') {
      // On slow CI, the outer Debugger.resume can be picked up in the same drain pass as
      // the Debugger.evaluateOnCallFrame, while V8 still considers the context paused.
      // In this case both resume calls may succeed and the process can continue running from
      // the setInterval until the timeout.
      expected.results.push({
        event: 'timeout',
        pending: [],
        error: {
          code: 'probe_timeout',
          message: 'Timed out after 30000ms waiting for target completion'
        },
      });
    }
    assertProbeJson(actual, expected);
  },
  trim: true,
});
