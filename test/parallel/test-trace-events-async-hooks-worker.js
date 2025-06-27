'use strict';

// This tests that enabling node.async_hooks in main threads also
// affects the workers.

const common = require('../common');
try {
  require('trace_events');
} catch {
  common.skip('missing trace events');
}

const assert = require('assert');
const cp = require('child_process');
const fs = require('fs');

const code =
  'setTimeout(() => { for (let i = 0; i < 100000; i++) { "test" + i } }, 1)';
const worker =
`const { Worker } = require('worker_threads');
const worker = new Worker('${code}',
{ eval: true, stdout: true, stderr: true });
worker.stdout.on('data',
  (chunk) => console.log('worker', chunk.toString()));
worker.stderr.on('data',
  (chunk) => console.error('worker', chunk.toString()));
worker.on('exit', () => { ${code} })`;

const tmpdir = require('../common/tmpdir');
const filename = tmpdir.resolve('node_trace.1.log');

tmpdir.refresh();
const proc = cp.spawnSync(
  process.execPath,
  [ '--trace-event-categories', 'node.async_hooks', '-e', worker ],
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
  console.log('Threads with Timeout traces:', threads);
  assert.strictEqual(threads.size, 2);
}
