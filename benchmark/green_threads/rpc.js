'use strict';
const common = require('../common.js');

const bench = common.createBenchmark(main, {
  direction: ['green-to-host', 'host-to-green'],
  concurrency: [1, 16],
  n: [10000],
}, {
  flags: ['--experimental-green-threads', '--no-warnings'],
});

// Makes `n` RPC calls in total, spread over `concurrency` green threads, each
// of which makes or receives one call at a time.
async function main({ direction, concurrency, n }) {
  const { GreenThread } = require('node:green_threads');
  const perThread = Math.ceil(n / concurrency);
  let started;
  const ready = new Promise((resolve) => { started = resolve; });
  const echoes = [];
  const greenThreads = Array.from({ length: concurrency }, () => {
    return new GreenThread(direction === 'green-to-host' ? `
      onmessage = async ({ data: n }) => {
        for (let i = 0; i < n; i++) await env.echo(i);
        postMessage(0);
      };
    ` : 'env.register((value) => value);', {
      env: {
        echo: (value) => value,
        register(echo) {
          echoes.push(echo);
          if (echoes.length === concurrency) started();
        },
      },
    });
  });
  if (direction === 'host-to-green') await ready;
  bench.start();
  await Promise.all(greenThreads.map(async (greenThread, index) => {
    if (direction === 'green-to-host') {
      const done = new Promise((resolve) => {
        greenThread.once('message', resolve);
      });
      greenThread.postMessage(perThread);
      await done;
    } else {
      const echo = echoes[index];
      for (let i = 0; i < perThread; i++) await echo(i);
    }
  }));
  bench.end(perThread * concurrency);
  await Promise.all(greenThreads.map((greenThread) => greenThread.terminate()));
}
