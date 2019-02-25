'use strict';

const common = require('../common');
try {
  require('trace_events');
} catch {
  common.skip('missing trace events');
}

const assert = require('assert');
const cp = require('child_process');
const fs = require('fs');
const path = require('path');
const util = require('util');

const code =
  'setTimeout(() => { for (var i = 0; i < 100000; i++) { "test" + i } }, 1)';
const worker = `const { Worker } = require('worker_threads');
  const worker = new Worker('${code}',
  { eval: true, stdout: true, stderr: true });
  worker.stdout.on('data',
    (chunk) => console.log('worker', chunk.toString()));
  worker.stderr.on('data',
    (chunk) => console.error('worker', chunk.toString()));`;

const tmpdir = require('../common/tmpdir');
const filename = path.join(tmpdir.path, 'node_trace.1.log');

tmpdir.refresh();
const proc = cp.spawnSync(
  process.execPath,
  [ '--trace-event-categories', 'node.async_hooks', '-e', worker ],
  {
    cwd: tmpdir.path,
    env: Object.assign({}, process.env, {
      'NODE_DEBUG_NATIVE': 'tracing',
      'NODE_DEBUG': 'tracing'
    })
  });

console.log(proc.signal);
console.log(proc.stderr.toString());
assert.strictEqual(proc.status, 0);

assert(fs.existsSync(filename));
const data = fs.readFileSync(filename, 'utf-8');
const traces = JSON.parse(data).traceEvents;
assert(traces.length > 0);
// V8 trace events should be generated.
assert(!traces.some((trace) => {
  if (trace.pid !== proc.pid)
    return false;
  if (trace.cat !== 'v8')
    return false;
  if (trace.name !== 'V8.ScriptCompiler')
    return false;
  return true;
}));

// C++ async_hooks trace events should be generated.
assert(traces.some((trace) => {
  if (trace.pid !== proc.pid)
    return false;
  if (trace.cat !== 'node,node.async_hooks')
    return false;
  return true;
}));

// JavaScript async_hooks trace events should be generated.
assert(traces.some((trace) => {
  if (trace.pid !== proc.pid)
    return false;
  if (trace.cat !== 'node,node.async_hooks')
    return false;
  if (trace.name !== 'Timeout')
    return false;
  return true;
}));

// Check args in init events
const initEvents = traces.filter((trace) => {
  return (trace.ph === 'b' && !trace.name.includes('_CALLBACK'));
});

for (const trace of initEvents) {
  if (trace.name === 'MESSAGEPORT' &&
    trace.args.data.executionAsyncId === 0 &&
    trace.args.data.triggerAsyncId === 0) {
    continue;
  }
  if (trace.args.data.executionAsyncId > 0 &&
    trace.args.data.triggerAsyncId > 0) {
    continue;
  }
  assert.fail('Unexpected initEvent: ',
              util.inspect(trace, { depth: Infinity }));
}
