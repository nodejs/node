'use strict';
const common = require('../common');
if (process.config.variables.node_without_node_options)
  common.skip('missing NODE_OPTIONS support');

// Test options specified by env variable.

const assert = require('assert');
const exec = require('child_process').execFile;

const tmpdir = require('../common/tmpdir');
tmpdir.refresh();

const printA = require.resolve('../fixtures/printA.js');
expect(`-r ${printA}`, 'A\nB\n');
expect(`-r ${printA} -r ${printA}`, 'A\nB\n');
expect(`   -r ${printA}    -r ${printA}`, 'A\nB\n');
expect(`   --require ${printA}    --require ${printA}`, 'A\nB\n');
expect('--no-deprecation', 'B\n');
expect('--no-warnings', 'B\n');
expect('--no_warnings', 'B\n');
expect('--trace-warnings', 'B\n');
expect('--redirect-warnings=_', 'B\n');
expect('--trace-deprecation', 'B\n');
expect('--trace-sync-io', 'B\n');
expect('--trace-events-enabled', 'B\n');
expect('--track-heap-objects', 'B\n');
expect('--throw-deprecation', 'B\n');
expect('--zero-fill-buffers', 'B\n');
expect('--v8-pool-size=10', 'B\n');
expect('--trace-event-categories node', 'B\n');
// eslint-disable-next-line no-template-curly-in-string
expect('--trace-event-file-pattern {pid}-${rotation}.trace_events', 'B\n');
// eslint-disable-next-line no-template-curly-in-string
expect('--trace-event-file-pattern {pid}-${rotation}.trace_events ' +
       '--trace-event-categories node.async_hooks', 'B\n');

if (!common.isWindows) {
  expect('--perf-basic-prof', 'B\n');
  expect('--perf-basic-prof-only-functions', 'B\n');
}

if (common.isLinux && ['arm', 'x64'].includes(process.arch)) {
  // PerfJitLogger is only implemented in Linux.
  expect('--perf-prof', 'B\n');
  expect('--perf-prof-unwinding-info', 'B\n');
}

if (common.hasCrypto) {
  expect('--use-openssl-ca', 'B\n');
  expect('--use-bundled-ca', 'B\n');
  expect('--openssl-config=_ossl_cfg', 'B\n');
}

// V8 options
expect('--abort_on-uncaught_exception', 'B\n');
expect('--max-old-space-size=0', 'B\n');
expect('--stack-trace-limit=100',
       /(\s*at f \(\[eval\]:1:\d*\)\r?\n){100}/,
       '(function f() { f(); })();',
       true);

function expect(opt, want, command = 'console.log("B")', wantsError = false) {
  const argv = ['-e', command];
  const opts = {
    cwd: tmpdir.path,
    env: Object.assign({}, process.env, { NODE_OPTIONS: opt }),
    maxBuffer: 1e6,
  };
  if (typeof want === 'string')
    want = new RegExp(want);
  exec(process.execPath, argv, opts, common.mustCall((err, stdout, stderr) => {
    if (wantsError) {
      stdout = stderr;
    } else {
      assert.ifError(err);
    }
    if (want.test(stdout)) return;

    const o = JSON.stringify(opt);
    assert.fail(`For ${o}, failed to find ${want} in: <\n${stdout}\n>`);
  }));
}
