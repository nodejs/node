'use strict';

// This test makes sure that when a domain is disposed, timers that are
// attached to that domain are not fired, but timers that are _not_ attached
// to that domain, including those whose callbacks are called from within
// the same invocation of listOnTimeout, _are_ called.

require('../common');
var assert = require('assert');
var domain = require('domain');
var disposalFailed = false;

// Repeatedly schedule a timer with a delay different than the timers attached
// to a domain that will eventually be disposed to make sure that they are
// called, regardless of what happens with those timers attached to domains
// that will eventually be disposed.
var a = 0;
log();
function log() {
  console.log(a++, process.domain);
  if (a < 10) setTimeout(log, 20);
}

var secondTimerRan = false;

// Use the same timeout duration for both "firstTimer" and "secondTimer"
// callbacks so that they are called during the same invocation of the
// underlying native timer's callback (listOnTimeout in lib/timers.js).
const TIMEOUT_DURATION = 50;

setTimeout(function firstTimer() {
  const d = domain.create();

  d.on('error', function handleError(err) {
    // Dispose the domain on purpose, so that we can test that nestedTimer
    // is not called since it's associated to this domain and a timer whose
    // domain is diposed should not run.
    d.dispose();
    console.error(err);
    console.error('in domain error handler',
                  process.domain, process.domain === d);
  });

  d.run(function() {
    // Create another nested timer that is by definition associated to the
    // domain "d". Because an error is thrown before the timer's callback
    // is called, and because the domain's error handler disposes the domain,
    // this timer's callback should never run.
    setTimeout(function nestedTimer() {
      console.error('Nested timer should not run, because it is attached to ' +
          'a domain that should be disposed.');
      disposalFailed = true;
      process.exit(1);
    });

    // Make V8 throw an unreferenced error. As a result, the domain's error
    // handler is called, which disposes the domain "d" and should prevent the
    // nested timer that is attached to it from running.
    err3(); // eslint-disable-line no-undef
  });
}, TIMEOUT_DURATION);

// This timer expires in the same invocation of listOnTimeout than firstTimer,
// but because it's not attached to any domain, it must run regardless of
// domain "d" being disposed.
setTimeout(function secondTimer() {
  console.log('In second timer');
  secondTimerRan = true;
}, TIMEOUT_DURATION);

process.on('exit', function() {
  assert.equal(a, 10);
  assert.equal(disposalFailed, false);
  assert(secondTimerRan);
  console.log('ok');
});
