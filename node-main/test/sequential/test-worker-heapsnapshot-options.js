// Flags: --expose-internals
'use strict';
const common = require('../common');
const { recordState, getHeapSnapshotOptionTests } = require('../common/heap');
const { Worker } = require('worker_threads');
const { once } = require('events');

(async function() {
  const tests = getHeapSnapshotOptionTests();
  const w = new Worker(tests.fixtures);

  await once(w, 'message');

  for (const { options, expected } of tests.cases) {
    const stream = await w.getHeapSnapshot(options);
    const snapshot = recordState(stream);
    tests.check(snapshot, expected);
  }

  await w.terminate();
})().then(common.mustCall());
