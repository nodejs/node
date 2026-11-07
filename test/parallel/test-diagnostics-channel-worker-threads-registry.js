'use strict';
const common = require('../common');
const assert = require('assert');
const { once } = require('events');
const dc = require('diagnostics_channel');
const { Worker, isMainThread, parentPort, workerData } = require('worker_threads');

const channelName = 'worker_threads.main.registry';

if (isMainThread) {
  // Expect one snapshot for each registration and removal in the worker tree.
  const snapshots = [];
  const childRegistered = Promise.withResolvers();
  const childRemoved = Promise.withResolvers();

  dc.subscribe(channelName, common.mustCall((message) => {
    assert.deepStrictEqual(Object.keys(message), ['threadIds']);
    snapshots.push(message.threadIds);

    if (snapshots.length === 2) {
      childRegistered.resolve();
    }
    if (snapshots.length === 3) {
      childRemoved.resolve();
    }
  }, 4));

  async function test() {
    const worker = new Worker(__filename);
    const threadId = worker.threadId;

    // The child ID and its registry update arrive on independent channels.
    const [[childId]] = await Promise.all([
      once(worker, 'message'),
      once(worker, 'online'),
      childRegistered.promise,
    ]);

    assert.deepStrictEqual(snapshots, [[threadId], [threadId, childId]]);

    // Remove the nested worker first, keeping its parent alive for the update.
    worker.postMessage('stop');
    await childRemoved.promise;

    assert.deepStrictEqual(snapshots, [[threadId], [threadId, childId], [threadId]]);

    // Let the parent exit naturally, then check that earlier snapshots are intact.
    const exited = once(worker, 'exit');
    worker.postMessage('finish');
    const [code] = await exited;

    assert.strictEqual(code, 0);
    assert.deepStrictEqual(snapshots, [[threadId], [threadId, childId], [threadId], []]);
  }

  test().then(common.mustCall(), common.mustNotCall());
} else {
  // Neither the parent worker nor its child should publish this channel locally.
  dc.subscribe(channelName, common.mustNotCall());

  if (workerData === 'child') {
    // Keep the child alive until its parent explicitly terminates it.
    parentPort.on('message', common.mustNotCall());
  } else {
    async function run() {
      const worker = new Worker(__filename, { workerData: 'child' });
      await once(worker, 'online');
      parentPort.postMessage(worker.threadId);

      // The main thread requests termination after observing both registrations.
      await once(parentPort, 'message');
      assert.strictEqual(await worker.terminate(), 1);

      // Wait until the main thread observes the child's removal before exiting.
      await once(parentPort, 'message');
      parentPort.close();
    }

    run().then(common.mustCall(), common.mustNotCall());
  }
}
