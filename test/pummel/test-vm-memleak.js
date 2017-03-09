'use strict';
// Flags: --max_old_space_size=32

require('../common');
const assert = require('assert');

const start = Date.now();
let maxMem = 0;

const ok = process.execArgv.some(function(arg) {
  return arg === '--max_old_space_size=32';
});
assert(ok, 'Run this test with --max_old_space_size=32.');

const interval = setInterval(function() {
  try {
    require('vm').runInNewContext('throw 1;');
  } catch (e) {
  }

  const rss = process.memoryUsage().rss;
  maxMem = Math.max(rss, maxMem);

  if (Date.now() - start > 5 * 1000) {
    // wait 10 seconds.
    clearInterval(interval);

    testContextLeak();
  }
}, 1);

function testContextLeak() {
  for (let i = 0; i < 1000; i++)
    require('vm').createContext({});
}

process.on('exit', function() {
  console.error('max mem: %dmb', Math.round(maxMem / (1024 * 1024)));
  assert.ok(maxMem < 64 * 1024 * 1024);
});
