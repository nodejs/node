'use strict';
// it.flaky('name', { flaky: 2 }, fn) must keep the explicit budget: the
// always-failing body runs exactly 3 times, not the boolean default of 21.
const { appendFileSync } = require('node:fs');
const { it } = require('node:test');

const state = process.env.FLAKY_STATE;

it.flaky('explicit budget is kept', { flaky: 2 }, () => {
  appendFileSync(state, 'x');
  throw new Error('always fails');
});
