// Flags: --experimental-workers --max-semi-space-size=1 --max-old-space-size=16 --max-executable-size=16

// Just something to run with valgrind, not real test

if (process.isMainInstance) {
  var assert = require('assert');
  var util = require('util');
  var Worker = require('worker');
  var checks = 0;

  var rss = function() {
    return process.memoryUsage().rss;
  };

  var l = 1;
  var cur = Promise.resolve();
  while(l--) {
    cur = cur.then(function() {
      var promises = [];
      var k = 4;
      while(k--)
        promises.push(runWorker());
      return Promise.all(promises);
    }).then(function() {
      console.log(rss());
    });
  }
}

process.on('unhandledRejection', function(e) {
  throw e;
});


function runWorker() {
  return new Promise(function(resolve) {
    new Worker(__filename, {keepAlive: false}).on('exit', resolve);
  });
}
