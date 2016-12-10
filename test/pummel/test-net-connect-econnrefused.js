'use strict';
// verify that connect reqs are properly cleaned up

var common = require('../common');
var assert = require('assert');
var net = require('net');

var ROUNDS = 10;
var ATTEMPTS_PER_ROUND = 100;
var rounds = 1;
var reqs = 0;

pummel();

function pummel() {
  console.log('Round', rounds, '/', ROUNDS);

  for (var pending = 0; pending < ATTEMPTS_PER_ROUND; pending++) {
    net.createConnection(common.PORT).on('error', function(err) {
      assert.equal(err.code, 'ECONNREFUSED');
      if (--pending > 0) return;
      if (rounds === ROUNDS) return check();
      rounds++;
      pummel();
    });
    reqs++;
  }
}

function check() {
  setTimeout(function() {
    assert.equal(process._getActiveRequests().length, 0);
    assert.equal(process._getActiveHandles().length, 1); // the timer
    check_called = true;
  }, 0);
}
var check_called = false;

process.on('exit', function() {
  assert.equal(rounds, ROUNDS);
  assert.equal(reqs, ROUNDS * ATTEMPTS_PER_ROUND);
  assert(check_called);
});
