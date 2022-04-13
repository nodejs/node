'use strict';
const common = require('../common');
const assert = require('assert');
const { AsyncLocalStorage } = require('async_hooks');

const als = new AsyncLocalStorage();

// Make sure _propagate function exists.
assert.ok(typeof als._propagate === 'function');

// The als instance should be getting removed from the storageList in
// lib/async_hooks.js when exit(...) is called, therefore when the nested runs
// are called there should be no copy of the als in the storageList to run the
// _propagate method on.
als._propagate = common.mustNotCall('_propagate() should not be called');

const done = common.mustCall();

function run(count) {
  if (count === 0) return done();
  als.run({}, () => {
    als.exit(run, --count);
  });
}
run(100);
