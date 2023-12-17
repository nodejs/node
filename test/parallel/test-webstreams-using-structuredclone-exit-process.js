'use strict';

const common = require('../common');
const { Worker } = require('worker_threads');
const { once } = require('node:events');

const worker = new Worker(
  `
  const { ReadableStream } = require('stream/web');
  const rs = new ReadableStream();
  const cloned = structuredClone(rs, { transfer: [rs] });
  `,
  { eval: true },
);

const worker2 = new Worker(
  `
  const { WritableStream } = require('stream/web');
  const ws = new WritableStream();
  const cloned = structuredClone(ws, { transfer: [ws] });
  `,
  { eval: true },
);

(async () => {
  // A timer is used here to detect the end of a process.
  const timer = setTimeout(common.mustNotCall(), common.platformTimeout(10000));

  await Promise.all([once(worker, 'exit'), once(worker2, 'exit')]);

  clearTimeout(timer);
})().then(common.mustCall());
