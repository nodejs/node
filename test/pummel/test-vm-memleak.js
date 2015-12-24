'use strict';
// Flags: --max_old_space_size=32

require('../common');
var assert = require('assert');

var start = Date.now();
var maxMem = 0;

var ok = process.execArgv.some(function(arg) {
  return arg === '--max_old_space_size=32';
});
assert(ok, 'Run this test with --max_old_space_size=32.');

var interval = setInterval(function() {
  try {
    require('vm').runInNewContext('throw 1;');
  } catch (e) {
  }

  var rss = process.memoryUsage().rss;
  maxMem = Math.max(rss, maxMem);

  if (Date.now() - start > 5 * 1000) {
    // wait 10 seconds.
    clearInterval(interval);

    testContextLeak();
  }
}, 1);

function testContextLeak() {
  for (var i = 0; i < 1000; i++)
    require('vm').createContext({});
}

process.on('exit', function() {
  console.error('max mem: %dmb', Math.round(maxMem / (1024 * 1024)));
  assert.ok(maxMem < 64 * 1024 * 1024);
});
