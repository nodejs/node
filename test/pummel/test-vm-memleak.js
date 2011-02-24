var assert = require('assert');
var common = require('../common');

var start = Date.now();
var maxMem = 0;

var interval = setInterval(function() {
  try {
    require('vm').runInNewContext('throw 1;');
  } catch(e) {
  }

  var rss = process.memoryUsage().rss;
  maxMem = Math.max(rss, maxMem);


  if (Date.now() - start > 5*1000) {
    // wait 10 seconds.
    clearInterval(interval);
  }
}, 1);

process.on('exit', function() {
  console.error('max mem: %dmb', Math.round(maxMem / (1024*1024)));
  // make sure we stay below 100mb
  assert.ok(maxMem < 50 * 1024 * 1024);
});
