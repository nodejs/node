// Tests that --timeout limits the startup of an attach session when the main thread of
// the target is blocked and does not answer Runtime.enable.
'use strict';

const common = require('../common');
common.skipIfInspectorDisabled();

const { runAttachedProbe } = require('../common/debugger-probe');

runAttachedProbe('probe-attach-blocked.js', [
  '--timeout', '1000', '--probe', 'probe-attach-blocked.js:5', '--expr', '1',
], {
  status: 1,
  report: {
    v: 2,
    probes: [{ expr: '1', target: { suffix: 'probe-attach-blocked.js', line: 5 }, locations: [] }],
    results: [{
      event: 'error',
      error: {
        code: 'probe_failure',
        message: 'Timed out after 1000ms waiting for the target to respond before probes: probe-attach-blocked.js:5. ' +
          'Its main thread may be blocked in synchronous work. Try increasing `--timeout`.',
        details: { lastCdpMethod: 'Runtime.enable' },
      },
      pending: [0],
    }],
  },
}).then(common.mustCall());
