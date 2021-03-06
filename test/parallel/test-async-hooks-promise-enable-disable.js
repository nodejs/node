'use strict';

const common = require('../common');
const assert = require('assert');
const async_hooks = require('async_hooks');
const EXPECTED_INITS = 2;
let p_er = null;
let p_inits = 0;

// Not useful to place common.mustCall() around 'exit' event b/c it won't be
// able to check it anyway.
process.on('exit', (code) => {
  if (code !== 0)
    return;
  if (p_er !== null)
    throw p_er;
  // Expecting exactly 2 PROMISE types to reach init.
  assert.strictEqual(p_inits, EXPECTED_INITS);
});

const mustCallInit = common.mustCall(function init(id, type, tid, resource) {
  if (type !== 'PROMISE')
    return;
  p_inits++;
}, EXPECTED_INITS);

const hook = async_hooks.createHook({
  init: mustCallInit
// Enable then disable to test whether disable() actually works.
}).enable().disable().disable();

new Promise(common.mustCall((res) => {
  res(42);
})).then(common.mustCall((val) => {
  hook.enable().enable();
  const p = new Promise((res) => res(val));
  hook.disable();
  return p;
})).then(common.mustCall((val2) => {
  hook.enable();
  const p = new Promise((res) => res(val2));
  hook.disable();
  return p;
})).catch((er) => p_er = er);
