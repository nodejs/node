'use strict';
const common = require('../common');
const assert = require('assert');
const { AsyncLocalStorage } = require('async_hooks');

const also = new AsyncLocalStorage();

// The _propagate function only exists on the old JavaScript implementation.
if (typeof also._propagate === 'function') {
  // The also instance should be getting removed from the storageList in
  // lib/async_hooks.js when exit(...) is called, therefore when the nested runs
  // are called there should be no copy of the also in the storageList to run the
  // _propagate method on.
  also._propagate = common.mustNotCall('_propagate() should not be called');
}

const done = common.mustCall();

const data = true;

function run(count) {
  if (count === 0) return done();
  assert.notStrictEqual(also.getStore(), data);
  also.run(data, () => {
    also.exit(run, --count);
  });
}
run(100);
