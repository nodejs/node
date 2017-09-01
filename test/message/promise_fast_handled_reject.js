'use strict';
const common = require('../common');
const assert = require('assert');

const p1 = new Promise((res, rej) => {
  consol.log('One'); // eslint-disable-line no-undef
});

const p2 = new Promise((res, rej) => { // eslint-disable-line no-unused-vars
  consol.log('Two'); // eslint-disable-line no-undef
});

const p3 = new Promise((res, rej) => {
  consol.log('Three'); // eslint-disable-line no-undef
});

new Promise((res, rej) => {
  setTimeout(common.mustCall(() => {
    p1.catch(() => {});
    p3.catch(() => {});
  }), 1);
});

process.on('rejectionHandled', () => {}); // Ignore

process.on('uncaughtException', (err) =>
  assert.fail('Should not trigger uncaught exception'));

process.on('exit', () => process._rawDebug('exit event emitted'));
