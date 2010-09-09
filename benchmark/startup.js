var spawn = require('child_process').spawn,
    path = require('path'),
    emptyJsFile = path.join(__dirname, '../test/fixtures/semicolon.js'),
    starts = 100,
    i = 0,
    start;

function startNode() {
  var node = spawn(process.execPath || process.argv[0], [emptyJsFile]);
  node.on('exit', function(exitCode) {
    if (exitCode !== 0) {
      throw new Error('Error during node startup');
    }

    i++;
    if (i < starts) {
      startNode();
    } else{
      var duration = +new Date - start;
      console.log('Started node %d times in %s ms. %d ms / start.', starts, duration, duration / starts);
    }
  });
}

start = +new Date;
startNode();
