'use strict';
const common = require('../common.js');
const { once } = require('events');

const bench = common.createBenchmark(main, {
  kind: ['green-thread', 'worker'],
  concurrency: [1, 16],
  n: [10000],
}, {
  flags: ['--experimental-green-threads', '--no-warnings'],
});

// Sends `n` messages in total, spread over `concurrency` threads, each of
// which echoes every message back before it gets the next one.
async function main({ kind, concurrency, n }) {
  let threads;
  if (kind === 'green-thread') {
    const { GreenThread } = require('node:green_threads');
    threads = Array.from({ length: concurrency }, () => new GreenThread(
      'onmessage = ({ data }) => postMessage(data);'));
  } else {
    const { Worker } = require('worker_threads');
    threads = Array.from({ length: concurrency }, () => new Worker(`
      const { parentPort } = require('worker_threads');
      parentPort.on('message', (data) => parentPort.postMessage(data));
    `, { eval: true }));
    await Promise.all(threads.map((w) => once(w, 'online')));
  }
  const perThread = Math.ceil(n / concurrency);
  bench.start();
  await Promise.all(threads.map(async (thread) => {
    for (let i = 0; i < perThread; i++) {
      const reply = once(thread, 'message');
      thread.postMessage(i);
      await reply;
    }
  }));
  bench.end(perThread * concurrency);
  await Promise.all(threads.map((thread) => thread.terminate()));
}
