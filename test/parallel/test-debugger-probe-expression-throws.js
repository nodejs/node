// This tests that a probe expression throwing an exception is recorded as
// a per-hit error and does not fail the session.
'use strict';

const common = require('../common');
common.skipIfInspectorDisabled();

const fixtures = require('../common/fixtures');
const { spawnSyncAndExit } = require('../common/child_process');
const { assertProbeJson } = require('../common/debugger-probe');

const cwd = fixtures.path('debugger');
const fixture = 'probe-throwing-getter.js';
const probes = [
  { expr: 'holder.throwingGetter', target: { suffix: fixture, line: 19 } },
  // This clears the interval so the process will exit naturally.
  { expr: 'markProbesDone()', target: { suffix: fixture, line: 20 } },
];
const url = fixtures.fileURL('debugger', fixture).href;

spawnSyncAndExit(process.execPath, [
  'inspect', '--json',
  '--probe', `${fixture}:19`, '--expr', probes[0].expr,
  '--probe', `${fixture}:20`, '--expr', probes[1].expr,
  fixture,
], { cwd, env: { ...process.env, NODE_DEBUG: 'inspect_probe' } }, {
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
        location: { url, line: 19, column: 3 },
        error: {
          message: 'Error: boom\n<stack>',
          details: {
            exception: {
              exceptionId: 1,
              text: 'Uncaught',
              lineNumber: 4,
              columnNumber: 4,
              scriptId: '<scriptId>',
              stackTrace: { callFrames: '<callFrames>' },
              exception: { type: 'object', subtype: 'error', description: 'Error: boom\n<stack>' },
            },
          },
        },
      }, {
        probe: 1,
        event: 'hit',
        hit: 1,
        location: { url, line: 20, column: 3 },
        result: { type: 'undefined' },
      }, {
        event: 'completed',
      }],
    });
  },
  trim: true,
});
