'use strict';

// Flags: --expose-internals
const common = require('../common');
const assert = require('assert');
const { Worker } = require('worker_threads');
const { HeapSnapshotStream } = require('internal/heap_utils');
const { kHandle } = require('internal/worker');

const worker = new Worker(__filename);
const snapShotResult = { ondone: () => {} };
worker[kHandle].takeHeapSnapshot = common.mustCall(() => snapShotResult);

worker.on('online', common.mustCall(() => {
  const snapShotResponse = worker.getHeapSnapshot();
  snapShotResult.ondone({});

  snapShotResponse.then(common.mustCall((heapSnapshotStream) => {
    assert.ok(heapSnapshotStream instanceof HeapSnapshotStream);
    worker.terminate();
  }));
}));
