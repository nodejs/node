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
      process.on('beforeExit', common.mustCall(() => {}));
    }));
}
