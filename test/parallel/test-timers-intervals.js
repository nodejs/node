var common = require('../common');
var assert = require('assert');

var ALLOWABLE_ERROR = 5; // [ms]

function nearly(a, b) {
  if (Math.abs(a-b) > ALLOWABLE_ERROR)
    assert.strictEqual(a, b);
}

function test(period, busyTime, expectedDelta, isUnref, next) {
  var res = [];

  var prev = Date.now();
  var interval = setInterval(function() {
    var s = Date.now();
    while (Date.now() - s < busyTime);
    var e = Date.now();

    res.push(s-prev);
    if (res.length === 5) {
      clearInterval(interval);
      done();
    }

    prev = e;
  }, period);

  if (isUnref) {
    interval.unref();
    var blocker = setTimeout(function() {
      assert(false, 'Interval works too long');
    }, Math.max(busyTime, period) * 6);
  }

  function done() {
    if (blocker) clearTimeout(blocker);
    nearly(period, res.shift());
    res.forEach(nearly.bind(null, expectedDelta));
    process.nextTick(next);
  }
}

test(50, 10, 40, false, function() { // ref, simple
  test(50, 10, 40, true, function() { // unref, simple
    test(50, 100, 0, false, function() { // ref, overlay
      test(50, 100, 0, true, function() {}); // unref, overlay
    });
  });
});
