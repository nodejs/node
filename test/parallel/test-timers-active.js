'use strict';
require('../common');
const assert = require('assert');
const active = require('timers').active;

// active() should create timers for these
const legitTimers = [
  { _idleTimeout: 0 },
  { _idleTimeout: 1 }
];

legitTimers.forEach(function(legit) {
  const savedTimeout = legit._idleTimeout;
  active(legit);
  // active() should mutate these objects
  assert.strictEqual(legit._idleTimeout, savedTimeout);
  assert(Number.isInteger(legit._idleStart));
  assert(legit._idleNext);
  assert(legit._idlePrev);
});


// active() should not create a timer for these
const bogusTimers = [
  { _idleTimeout: -1 },
  { _idleTimeout: undefined },
];

bogusTimers.forEach(function(bogus) {
  const savedTimeout = bogus._idleTimeout;
  active(bogus);
  // active() should not mutate these objects
  assert.deepStrictEqual(bogus, { _idleTimeout: savedTimeout });
});
