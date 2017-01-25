'use strict';
// verify that connect reqs are properly cleaned up

const common = require('../common');
const assert = require('assert');
const net = require('net');

const ROUNDS = 10;
const ATTEMPTS_PER_ROUND = 100;
let rounds = 1;
let reqs = 0;

pummel();

function pummel() {
  console.log('Round', rounds, '/', ROUNDS);

  let pending;
  for (pending = 0; pending < ATTEMPTS_PER_ROUND; pending++) {
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
let check_called = false;

process.on('exit', function() {
  assert.equal(rounds, ROUNDS);
  assert.equal(reqs, ROUNDS * ATTEMPTS_PER_ROUND);
  assert(check_called);
});
