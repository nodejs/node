'use strict';

const common = require('../common');
const assert = require('assert');
let allsGood = false;
let cntr = 0;

process.on('exit', () => {
  assert.ok(cntr > 0, '_tickDomainCallback was never called');
});

/**
 * This test relies upon the following internals to work as specified:
 *  - require('domain') causes node::Environment::set_tick_callback_function()
 *    to use process._tickDomainCallback() to process the nextTickQueue;
 *    replacing process._tickCallback().
 *  - setImmediate() uses node::MakeCallback() instead of
 *    node::AsyncWrap::MakeCallback(). Otherwise the test will always pass.
 *    Have not found a way to verify that node::MakeCallback() is used.
 */
process._tickDomainCallback = function _tickDomainCallback() {
  assert.ok(allsGood, '_tickDomainCallback should not have been called');
  cntr++;
};

setImmediate(common.mustCall(() => {
  require('domain');
  setImmediate(common.mustCall(() => setImmediate(common.mustCall(() => {
    allsGood = true;
    process.nextTick(() => {});
  }))));
}));
