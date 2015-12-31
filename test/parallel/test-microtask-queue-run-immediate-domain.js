'use strict';
require('../common');
var assert = require('assert');

// Requiring the domain module here changes the function that is used by node to
// call process.nextTick's callbacks to a variant that specifically handles
// domains. We want to test this specific variant in this test, and so even if
// the domain module is not used, this require call is needed and must not be
// removed.
require('domain');

function enqueueMicrotask(fn) {
  Promise.resolve().then(fn);
}

var done = 0;

process.on('exit', function() {
  assert.equal(done, 2);
});

// no nextTick, microtask
setImmediate(function() {
  enqueueMicrotask(function() {
    done++;
  });
});


// no nextTick, microtask with nextTick
setImmediate(function() {
  var called = false;

  enqueueMicrotask(function() {
    process.nextTick(function() {
      called = true;
    });
  });

  setImmediate(function() {
    if (called)
      done++;
  });

});
