'use strict';
const common = require('../common');

const p1 = new Promise((res, rej) => {
  consol.log('One'); // eslint-disable-line no-undef
});

const p2 = new Promise((res, rej) => { //eslint-disable-line no-unused-vars
  consol.log('Two'); // eslint-disable-line no-undef
});

const p3 = new Promise((res, rej) => {
  consol.log('Three'); // eslint-disable-line no-undef
});

new Promise((res, rej) => {
  setTimeout(common.mustCall(() => {
    p1.catch(() => {});
    p3.catch(() => {});
  }));
});

process.on('uncaughtException', (err) =>
    common.fail('Should not trigger uncaught exception'));

process.on('exit', () => process._rawDebug('exit event emitted'));
