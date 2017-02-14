'use strict';
require('../common');
const assert = require('assert');

// Requiring the domain module here changes the function that is used by node to
// call process.nextTick's callbacks to a variant that specifically handles
// domains. We want to test this specific variant in this test, and so even if
// the domain module is not used, this require call is needed and must not be
// removed.
require('domain');

const implementations = [
  function(fn) {
    Promise.resolve().then(fn);
  }
];

let expected = 0;
let done = 0;

process.on('exit', function() {
  assert.strictEqual(done, expected);
});

function test(scheduleMicrotask) {
  let nextTickCalled = false;
  expected++;

  scheduleMicrotask(function() {
    process.nextTick(function() {
      nextTickCalled = true;
    });

    setTimeout(function() {
      assert(nextTickCalled);
      done++;
    }, 0);
  });
}

// first tick case
implementations.forEach(test);

// tick callback case
setTimeout(function() {
  implementations.forEach(function(impl) {
    process.nextTick(test.bind(null, impl));
  });
}, 0);
