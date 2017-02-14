'use strict';
require('../common');
const assert = require('assert');

// setImmediate should run clear its queued cbs once per event loop turn
// but immediates queued while processing the current queue should happen
// on the next turn of the event loop.

// hit should be the exact same size of QUEUE, if we're letting things
// recursively add to the immediate QUEUE hit will be > QUEUE

let ticked = false;

let hit = 0;
const QUEUE = 1000;

function run() {
  if (hit === 0)
    process.nextTick(function() { ticked = true; });

  if (ticked) return;

  hit += 1;
  setImmediate(run);
}

for (let i = 0; i < QUEUE; i++)
  setImmediate(run);

process.on('exit', function() {
  console.log('hit', hit);
  assert.strictEqual(hit, QUEUE, 'We ticked between the immediate queue');
});
