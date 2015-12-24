'use strict';
require('../common');
var assert = require('assert');

// setImmediate should run clear its queued cbs once per event loop turn
// but immediates queued while processing the current queue should happen
// on the next turn of the event loop.

// in v0.10 hit should be 1, because we only process one cb per turn
// in v0.11 and beyond it should be the exact same size of QUEUE
// if we're letting things recursively add to the immediate QUEUE hit will be
// > QUEUE

var ticked = false;

var hit = 0;
var QUEUE = 1000;

function run() {
  if (hit === 0)
    process.nextTick(function() { ticked = true; });

  if (ticked) return;

  hit += 1;
  setImmediate(run);
}

for (var i = 0; i < QUEUE; i++)
  setImmediate(run);

process.on('exit', function() {
  console.log('hit', hit);
  assert.strictEqual(hit, QUEUE, 'We ticked between the immediate queue');
});
