'use strict';
const common = require('../common');

common.skipIfInspectorDisabled();

const assert = require('assert');
const inspector = require('inspector');
const stream = require('stream');
const { Worker, workerData } = require('worker_threads');

const session = new inspector.Session();
session.connect();
session.post('HeapProfiler.enable');
session.post('HeapProfiler.startTrackingHeapObjects',
             { trackAllocations: true });

// Perform some silly heap allocations for the next 100 ms.
const interval = setInterval(() => {
  new stream.PassThrough().end('abc').on('data', common.mustCall());
}, 1);

setTimeout(() => {
  clearInterval(interval);

  // Once the main test is done, we re-run it from inside a Worker thread
  // and stop early, as that is a good way to make sure the timer handles
  // internally created by the inspector are cleaned up properly.
  if (workerData === 'stopEarly')
    process.exit();

  let data = '';
  session.on('HeapProfiler.addHeapSnapshotChunk',
             common.mustCallAtLeast((event) => {
               data += event.params.chunk;
             }));

  // TODO(addaleax): Using `{ reportProgress: true }` crashes the process
  // because the progress indication event would mean calling into JS while
  // a heap snapshot is being taken, which is forbidden.
  // What can we do about that?
  session.post('HeapProfiler.stopTrackingHeapObjects');

  assert(data.includes('PassThrough'), data);

  new Worker(__filename, { workerData: 'stopEarly' });
}, 100);
