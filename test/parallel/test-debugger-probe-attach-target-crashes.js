// Tests that if the target is terminated by an uncaught exception, the attach session reports
// probe_target_exit with the exception, instead of as completed.
'use strict';

const common = require('../common');
common.skipIfInspectorDisabled();

const assert = require('assert');
const fixtures = require('../common/fixtures');
const { runProbe, startTarget } = require('../common/debugger-probe');

const url = fixtures.fileURL('debugger', 'probe-attach-finishes.js').href;
const path = fixtures.path('debugger', 'probe-attach-finishes.js');

async function main() {
  const target = await startTarget('probe-attach-finishes.js', { env: { PROBE_ATTACH_THROW: '1' } });
  // Closing its stdin makes the target run line 5 once and throw. Line 11 only runs at startup.
  await runProbe([
    '--probe', 'probe-attach-finishes.js:5', '--expr', 'count',
    '--probe', 'probe-attach-finishes.js:11', '--expr', '1',
    '--host', target.host, '--port', `${target.port}`,
  ], {
    target,
    onStarted: () => target.child.stdin.end(),
    status: 0,
    report: {
      v: 2,
      probes: [
        { expr: 'count', target: { suffix: 'probe-attach-finishes.js', line: 5 } },
        { expr: '1', target: { suffix: 'probe-attach-finishes.js', line: 11 } },
      ],
      results: [{
        probe: 0, event: 'hit', hit: 1, result: { type: 'number', value: 0, description: '0' },
        location: { url, line: 5, column: 3 },
      }, {
        event: 'error',
        pending: [1],
        error: {
          code: 'probe_target_exit',
          message: 'Target exited with an uncaught exception before probes: probe-attach-finishes.js:11',
          details: {
            exception: {
              exceptionId: 1,
              text: 'Uncaught',
              lineNumber: 5,
              columnNumber: 37,
              scriptId: '<scriptId>',
              url: path,
              stackTrace: { callFrames: '<callFrames>' },
              exception: { type: 'object', subtype: 'error', description: 'Error: boom\n<stack>' },
              executionContextId: 1,
            },
          },
        },
      }],
    },
  });
  assert.deepStrictEqual(await target.exited, { code: 1, signal: null });
}

main().then(common.mustCall());
