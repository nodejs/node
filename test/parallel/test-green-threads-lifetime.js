// Flags: --experimental-green-threads --no-warnings
'use strict';

const common = require('../common');
const assert = require('assert');
const { spawnSyncAndAssert } = require('../common/child_process');
const { Worker } = require('worker_threads');

// Green threads that are still running are terminated when the host
// Environment is torn down.
spawnSyncAndAssert(process.execPath, [
  '--experimental-green-threads',
  '--no-warnings',
  '-e',
  `
  const { GreenThread } = require('node:green_threads');
  new GreenThread('for (;;);');
  new GreenThread('setInterval(() => {}, 1);');
  const unrefed = new GreenThread('setInterval(() => {}, 1);').unref();
  setTimeout(() => process.exit(0), 10);
  `,
], { status: 0, stdout: '', stderr: '' });

// unref() lets the process exit while the green thread is running.
spawnSyncAndAssert(process.execPath, [
  '--experimental-green-threads',
  '--no-warnings',
  '-e',
  `
  const { GreenThread } = require('node:green_threads');
  new GreenThread('setInterval(() => {}, 1);').unref();
  new GreenThread('setInterval(() => {}, 1);').unref().ref().unref();
  `,
], { status: 0, stdout: '', stderr: '' });

// A running green thread keeps the event loop alive until it exits.
spawnSyncAndAssert(process.execPath, [
  '--experimental-green-threads',
  '--no-warnings',
  '-e',
  `
  const { GreenThread } = require('node:green_threads');
  const greenThread = new GreenThread('setTimeout(() => close(), 20);');
  greenThread.on('exit', (code) => console.log('exit', code));
  `,
], { status: 0, stdout: 'exit 0\n', stderr: '' });

// Workers can host green threads, and terminating the worker terminates them.
{
  const worker = new Worker(`
    const { parentPort } = require('worker_threads');
    const { GreenThread } = require('node:green_threads');
    const greenThread = new GreenThread(
      'onmessage = ({ data }) => postMessage(data * 3);');
    greenThread.on('message', (message) => parentPort.postMessage(message));
    greenThread.postMessage(14);
    new GreenThread('for (;;);');
  `, { eval: true });
  worker.on('message', common.mustCall((message) => {
    assert.strictEqual(message, 42);
    worker.terminate();
  }));
  worker.on('exit', common.mustCall());
}
