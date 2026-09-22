// Tests that when a probe session starts by attaching to a process with --pid,
// which activates inspector in the target process, should stop the probe session after
// reaching --max-hit.
'use strict';

const common = require('../common');
common.skipIfInspectorDisabled();

const fixtures = require('../common/fixtures');
const { runProbe, startTarget } = require('../common/debugger-probe');

async function main() {
  const target = await startTarget('probe-attach-loop.js', { args: [`--inspect-port=${common.PORT}`] });
  const { pid } = target.child;
  const attachArgs = ['--pid', `${pid}`, '--port', `${common.PORT}`];
  await runProbe(['--probe', 'probe-attach-loop.js:6', '--expr', 'typeof count', '--max-hit', '1', ...attachArgs], {
    target,
    status: 0,
    report: {
      v: 2,
      target: { pid, host: '127.0.0.1', port: common.PORT },
      probes: [{ expr: 'typeof count', target: { suffix: 'probe-attach-loop.js', line: 6 }, maxHit: 1 }],
      results: [{
        probe: 0, event: 'hit', hit: 1, result: { type: 'string', value: 'number' },
        location: { url: fixtures.fileURL('debugger', 'probe-attach-loop.js').href, line: 6, column: 3 },
      }, { event: 'completed' }],
    },
  });

  // The target should be left running after --max-hit is reached and it is detached.
  // We should be able to attach a second session to it. Killing the target at this point
  // should result in a probe_target_exit with exit code 0
  // Line 10 only runs at startup, so this session should wait until the target is killed.
  await runProbe(['--probe', 'probe-attach-loop.js:10', '--expr', '1', ...attachArgs], {
    target,
    onStarted: () => target.child.kill('SIGKILL'),
    status: 0,
    report: {
      v: 2,
      probes: [{ expr: '1', target: { suffix: 'probe-attach-loop.js', line: 10 } }],
      results: [{
        event: 'error',
        pending: [0],
        error: {
          code: 'probe_target_exit',
          message: 'Target exited before probes: probe-attach-loop.js:10',
        },
      }],
    },
  });
  await target.exited;
}

main().then(common.mustCall());
