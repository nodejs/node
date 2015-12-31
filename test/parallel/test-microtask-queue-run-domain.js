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
setTimeout(function() {
  enqueueMicrotask(function() {
    done++;
  });
}, 0);


// no nextTick, microtask with nextTick
setTimeout(function() {
  var called = false;

  enqueueMicrotask(function() {
    process.nextTick(function() {
      called = true;
    });
  });

  setTimeout(function() {
    if (called)
      done++;
  }, 0);

}, 0);
