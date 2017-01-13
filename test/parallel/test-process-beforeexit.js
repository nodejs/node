'use strict';
const common = require('../common');
const net = require('net');

process.once('beforeExit', common.mustCall(tryImmediate));

function tryImmediate() {
  setImmediate(common.mustCall(() => {
    process.once('beforeExit', common.mustCall(tryTimer));
  }));
}

function tryTimer() {
  setTimeout(common.mustCall(() => {
    process.once('beforeExit', common.mustCall(tryListen));
  }), 1);
}

function tryListen() {
  net.createServer()
    .listen(0)
    .on('listening', common.mustCall(function() {
      this.close();
      process.once('beforeExit', common.mustCall(tryRepeatedTimer));
    }));
}

// test that a function invoked from the beforeExit handler can use a timer
// to keep the event loop open, which can use another timer to keep the event
// loop open, etc.
function tryRepeatedTimer() {
  const N = 5;
  let n = 0;
  const repeatedTimer = common.mustCall(function() {
    if (++n < N)
      setTimeout(repeatedTimer, 1);
  }, N);
  setTimeout(repeatedTimer, 1);
}
