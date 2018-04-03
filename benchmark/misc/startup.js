'use strict';
const common = require('../common.js');
const spawn = require('child_process').spawn;
const path = require('path');
const emptyJsFile = path.resolve(__dirname, '../../test/fixtures/semicolon.js');

const bench = common.createBenchmark(startNode, {
  dur: [1]
});

function startNode({ dur }) {
  var go = true;
  var starts = 0;

  setTimeout(function() {
    go = false;
  }, dur * 1000);

  bench.start();
  start();

  function start() {
    const node = spawn(process.execPath || process.argv[0], [emptyJsFile]);
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
