'use strict';
require('../common');

process.on('uncaughtException', function() { });

setTimeout(function() {
  process.nextTick(function() {
    var c = setInterval(function() {
      clearInterval(c);
      throw new Error('setInterval');
    });
  });
  setTimeout(function() {
    throw new Error('setTimeout');
  });
});
