'use strict';
require('../common');
var assert = require('assert');

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
