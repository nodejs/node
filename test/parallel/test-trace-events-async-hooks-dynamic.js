'use strict';

// This tests that tracing can be enabled dynamically with the
// trace_events module.

const common = require('../common');
try {
  require('trace_events');
} catch {
  common.skip('missing trace events');
}

const assert = require('assert');
const cp = require('child_process');
const fs = require('fs');

const enable = `require("trace_events").createTracing(
{ categories: ["node.async_hooks"] }).enable();`;
const code =
  'setTimeout(() => { for (let i = 0; i < 100000; i++) { "test" + i } }, 1)';

const tmpdir = require('../common/tmpdir');
const filename = tmpdir.resolve('node_trace.1.log');

tmpdir.refresh();
const proc = cp.spawnSync(
  process.execPath,
  ['-e', enable + code ],
  {
    cwd: tmpdir.path,
    env: { ...process.env,
           'NODE_DEBUG_NATIVE': 'tracing',
           'NODE_DEBUG': 'tracing' }
  });

console.log('process exit with signal:', proc.signal);
console.log('process stderr:', proc.stderr.toString());

assert.strictEqual(proc.status, 0);
assert(fs.existsSync(filename));
const data = fs.readFileSync(filename, 'utf-8');
const traces = JSON.parse(data).traceEvents;

function filterTimeoutTraces(trace) {
  if (trace.pid !== proc.pid)
    return false;
  if (trace.cat !== 'node,node.async_hooks')
    return false;
  if (trace.name !== 'Timeout')
    return false;
  return true;
}

{
  const timeoutTraces = traces.filter(filterTimeoutTraces);
  assert.notDeepStrictEqual(timeoutTraces, []);
  const threads = new Set();
  for (const trace of timeoutTraces) {
    threads.add(trace.tid);
  }
  assert.notDeepStrictEqual(timeoutTraces, []);
}
