'use strict';

const common = require('../common');
const assert = require('assert');

const p1 = new Promise((res, rej) => {
  throw new Error('One');
});

new Promise((res, rej) => {
  throw new Error('Two');
});

const p3 = new Promise((res, rej) => {
  throw new Error('Three');
});

process.nextTick(() => {
  setTimeout(common.mustCall(() => {
    p1.catch(() => {});
    p3.catch(() => {});
  }), 1);
});

process.on('rejectionHandled', () => {}); // Ignore

process.on('uncaughtException', (err) =>
  assert.fail('Should not trigger uncaught exception'));

process.on('exit', () => process._rawDebug('exit event emitted'));
