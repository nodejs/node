var common = require('../common.js');
var spawn = require('child_process').spawn;
var path = require('path');
var emptyJsFile = path.resolve(__dirname, '../../test/fixtures/semicolon.js');
var starts = 100;
var i = 0;
var start;

var bench = common.createBenchmark(startNode, {
  dur: [1]
});

function startNode(conf) {
  var dur = +conf.dur;
  var go = true;
  var starts = 0;
  var open = 0;

  setTimeout(function() {
    go = false;
  }, dur * 1000);

  bench.start();
  start();

  function start() {
    var node = spawn(process.execPath || process.argv[0], [emptyJsFile]);
    node.on('exit', function(exitCode) {
      if (exitCode !== 0) {
        throw new Error('Error during node startup');
      }
      starts++;

      if (go)
        start();
      else
        bench.end(starts);
    });
  }
}
