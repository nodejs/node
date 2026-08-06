// Heap profile labels require async-context-frame (on by default).
'use strict';
// Test that terminating a worker thread while heap profiling is active
// does not crash.  The cleanup hook in node_v8.cc must clear the V8
// HeapProfiler callback and disable allocator tracking before the
// Isolate is disposed.
const common = require('../common');
const assert = require('assert');
const { Worker } = require('worker_threads');

// Test 1: Worker starts profiling, is terminated without stopping profiler.
{
  const worker = new Worker(`
    const v8 = require('v8');
    const { parentPort } = require('worker_threads');

    v8.startHeapProfile({ sampleInterval: 64, labels: true });

    // Allocate some objects to generate samples
    const arr = [];
    for (let i = 0; i < 500; i++) arr.push({ x: i });

    // Signal that profiling is active
    parentPort.postMessage('profiling');

    // Keep worker alive until terminated
    setTimeout(() => {}, 60000);
  `, { eval: true });

  worker.on('message', common.mustCall((msg) => {
    assert.strictEqual(msg, 'profiling');
    // Terminate the worker while profiling is still active
    worker.terminate();
  }));

  worker.on('exit', common.mustCall((code) => {
    assert.strictEqual(code, 1);
  }));
}

// Test 2: Worker starts profiling with labels, is terminated.
{
  const worker = new Worker(`
    const v8 = require('v8');
    const { parentPort } = require('worker_threads');

    v8.startHeapProfile({ sampleInterval: 64, labels: true });

    v8.withHeapProfileLabels({ route: '/worker' }, () => {
      const arr = [];
      for (let i = 0; i < 500; i++) arr.push({ x: i });

      // Signal that labeled profiling is active
      parentPort.postMessage('labeled');

      // Keep worker alive until terminated
      setTimeout(() => {}, 60000);
    });
  `, { eval: true });

  worker.on('message', common.mustCall((msg) => {
    assert.strictEqual(msg, 'labeled');
    worker.terminate();
  }));

  worker.on('exit', common.mustCall((code) => {
    assert.strictEqual(code, 1);
  }));
}

// Test 3: Worker starts and stops profiling normally, then exits.
{
  const worker = new Worker(`
    const v8 = require('v8');
    const { parentPort } = require('worker_threads');

    const handle = v8.startHeapProfile({ sampleInterval: 64, labels: true });
    const arr = [];
    for (let i = 0; i < 500; i++) arr.push({ x: i });
    const profile = handle.getAllocationProfile();
    handle.stop();

    parentPort.postMessage({
      hasSamples: profile && profile.samples && profile.samples.length > 0
    });
  `, { eval: true });

  worker.on('message', common.mustCall((msg) => {
    assert.ok(msg.hasSamples, 'Worker profiling should produce samples');
  }));

  worker.on('exit', common.mustCall((code) => {
    assert.strictEqual(code, 0);
  }));
}
