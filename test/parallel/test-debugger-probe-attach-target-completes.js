// Tests that an attach session exits normally when the target finishes on its own, instead of
// waiting for --timeout. Probes that never are hit should be reported as missed.
'use strict';

const common = require('../common');
common.skipIfInspectorDisabled();

const assert = require('assert');
const fixtures = require('../common/fixtures');
const { runProbe, startTarget } = require('../common/debugger-probe');

async function main() {
  const target = await startTarget('probe-attach-finishes.js');
  // Closing its stdin makes the target run line 5 once and exit. Line 11 only runs at startup.
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
        location: { url: fixtures.fileURL('debugger', 'probe-attach-finishes.js').href, line: 5, column: 3 },
      }, { event: 'miss', pending: [1] }],
    },
  });
  assert.deepStrictEqual(await target.exited, { code: 0, signal: null });
}

main().then(common.mustCall());
