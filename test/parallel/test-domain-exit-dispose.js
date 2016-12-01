'use strict';
require('../common');
var common = require('../common');
var assert = require('assert');
var domain = require('domain');

// no matter what happens, we should increment a 10 times.
var a = 0;
log();
function log() {
  console.log(a++, process.domain);
  if (a < 10) setTimeout(log, 20);
}

// in 50ms we'll throw an error.
setTimeout(err, 50);
function err() {
  var d = domain.create();
  d.on('error', handle);
  d.run(err2);

  function err2() {
    // this timeout should never be called, since the domain gets
    // disposed when the error happens.
    setTimeout(common.mustCall(() => {}, 0), 1);

    // this function doesn't exist, and throws an error as a result.
    err3(); // eslint-disable-line no-undef
  }

  function handle(e) {
    // this should clean up everything properly.
    d.dispose();
    console.error(e);
    console.error('in handler', process.domain, process.domain === d);
  }
}

process.on('exit', function() {
  assert.strictEqual(a, 10);
  console.log('ok');
});
