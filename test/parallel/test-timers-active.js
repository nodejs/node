'use strict';
const common = require('../common');
const assert = require('assert');
const active = require('timers').active;

// active() should not create a timer for these
var legitTimers = [
  { _idleTimeout: 0 },
  { _idleTimeout: 1 }
];

legitTimers.forEach(function(legit) {
  active(legit);
  // active() should mutate these objects
  assert.notDeepEqual(Object.keys(legit), ['_idleTimeout']);
});


// active() should not create a timer for these
var bogusTimers = [
  { _idleTimeout: -1 }
];

bogusTimers.forEach(function(bogus) {
  active(bogus);
  // active() should not mutate these objects
  assert.deepEqual(Object.keys(bogus), ['_idleTimeout']);
});
