'use strict';
var common = require('../common');
var assert = require('assert');

// this is the inverse of test-next-tick-starvation.
// it verifies that process.nextTick will *always* come before other
// events, up to the limit of the process.maxTickDepth value.

// WARNING: unsafe!
process.maxTickDepth = Infinity;

var ran = false;
var starved = false;
var start = +new Date();
var timerRan = false;

function spin() {
  ran = true;
  var now = +new Date();
  if (now - start > 100) {
    console.log('The timer is starving, just as we planned.');
    starved = true;

    // now let it out.
    return;
  }

  process.nextTick(spin);
}

function onTimeout() {
  if (!starved) throw new Error('The timer escaped!');
  console.log('The timer ran once the ban was lifted');
  timerRan = true;
}

spin();
setTimeout(onTimeout, 50);

process.on('exit', function() {
  assert.ok(ran);
  assert.ok(starved);
  assert.ok(timerRan);
});
