'use strict';

require('../common');

const { scheduler } = require('timers/promises');
const { throws } = require('assert');

throws(() => {
  const kScheduler = Object.getOwnPropertySymbols(scheduler)[0];
  delete scheduler[kScheduler];
  scheduler.yield();
}, {
  code: 'ERR_INVALID_THIS'
});
