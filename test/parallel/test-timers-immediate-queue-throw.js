'use strict';

const common = require('../common');
const assert = require('assert');
const domain = require('domain');

// setImmediate should run clear its queued cbs once per event loop turn
// but immediates queued while processing the current queue should happen
// on the next turn of the event loop.

// In addition, if any setImmediate throws, the rest of the queue should
// be processed after all error handling is resolved, but that queue
// should not include any setImmediate calls scheduled after the
// processing of the queue started.

let threw = false;
let stage = -1;

const QUEUE = 10;

const errObj = {
  type: Error,
  message: 'setImmediate Err'
};

process.once('uncaughtException', common.expectsError(errObj));
process.once('uncaughtException', () => assert.strictEqual(stage, 0));

const d1 = domain.create();
d1.once('error', common.expectsError(errObj));
d1.once('error', () => assert.strictEqual(stage, 0));

const run = common.mustCall((callStage) => {
  assert(callStage >= stage);
  stage = callStage;
  if (threw)
    return;

  setImmediate(run, 2);
}, QUEUE * 3);

for (let i = 0; i < QUEUE; i++)
  setImmediate(run, 0);
setImmediate(() => {
  threw = true;
  process.nextTick(() => assert.strictEqual(stage, 1));
  throw new Error('setImmediate Err');
});
d1.run(() => setImmediate(() => {
  throw new Error('setImmediate Err');
}));
for (let i = 0; i < QUEUE; i++)
  setImmediate(run, 1);
