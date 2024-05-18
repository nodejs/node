'use strict';

require('../common');

const { scheduler } = require('timers/promises');
const { throws } = require('assert');

throws(() => {
  const kScheduler = Object.getOwnPropertySymbols(scheduler)[0];
  delete scheduler[kScheduler];
  scheduler.wait();
}, {
  code: 'ERR_INVALID_THIS'
});
