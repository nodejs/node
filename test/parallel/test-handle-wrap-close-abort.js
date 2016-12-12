'use strict';
const common = require('../common');

function noop() {}
const uncaughtExceptionHandler = common.mustCall(noop, 2);

process.on('uncaughtException', uncaughtExceptionHandler);

setTimeout(function() {
  process.nextTick(function() {
    const c = setInterval(function() {
      clearInterval(c);
      throw new Error('setInterval');
    }, 1);
  });
  setTimeout(function() {
    throw new Error('setTimeout');
  }, 1);
});
