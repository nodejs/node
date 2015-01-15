var common = require('../common');
var assert = require('assert');
var spawn = require('child_process').spawn;

var args = [ common.fixturesDir + '/clustered-server/app.js' ];
var child = spawn(process.execPath, args, {
  stdio: [ 'pipe', 'pipe', 'pipe', 'ipc' ]
});
var outputLines = [];
var outputTimerId;
var waitingForDebuggers = false;

var pids = null;

child.stderr.on('data', function(data) {
  var lines = data.toString().replace(/\r/g, '').trim().split('\n');

  lines.forEach(function(line) {
    console.log('> ' + line);

    if (line === 'all workers are running') {
      child.on('message', function(msg) {
        if (msg.type !== 'pids')
          return;

        pids = msg.pids;
        console.error('got pids %j', pids);

        waitingForDebuggers = true;
        process._debugProcess(child.pid);
      });

      child.send({
        type: 'getpids'
      });
    } else if (waitingForDebuggers) {
      outputLines.push(line);
    }

  });
  if (outputLines.length >= expectedLines.length)
    onNoMoreLines();
});

function onNoMoreLines() {
  assertOutputLines();
  process.exit();
}

setTimeout(function testTimedOut() {
  assert(false, 'test timed out.');
}, 6000).unref();

process.on('exit', function onExit() {
  // Kill processes in reverse order to avoid timing problems on Windows where
  // the parent process is killed before the children.
  pids.reverse().forEach(function(pid) {
    process.kill(pid);
  });
});

var expectedLines = [
  'Starting debugger agent.',
  'Debugger listening on port ' + 5858,
  'Starting debugger agent.',
  'Debugger listening on port ' + 5859,
  'Starting debugger agent.',
  'Debugger listening on port ' + 5860,
];

function assertOutputLines() {
  // Do not assume any particular order of output messages,
  // since workers can take different amout of time to
  // start up
  outputLines.sort();
  expectedLines.sort();

  assert.equal(outputLines.length, expectedLines.length);
  for (var i = 0; i < expectedLines.length; i++)
    assert.equal(outputLines[i], expectedLines[i]);
}
