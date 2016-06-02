'use strict';
const common = require('../common');
var assert = require('assert');
var os = require('os');

if (os.type() != 'SunOS') {
  common.skip('no DTRACE support');
  return;
}

/*
 * Some functions to create a recognizable stack.
 */
var frames = [ 'stalloogle', 'bagnoogle', 'doogle' ];

var stalloogle = function(str) {
  global.expected = str;
  os.loadavg();
};

var bagnoogle = function(arg0, arg1) {
  stalloogle(arg0 + ' is ' + arg1 + ' except that it is read-only');
};

var done = false;

var doogle = function() {
  if (!done)
    setTimeout(doogle, 10);

  bagnoogle('The bfs command', '(almost) like ed(1)');
};

var spawn = require('child_process').spawn;

/*
 * We're going to use DTrace to stop us, gcore us, and set us running again
 * when we call getloadavg() -- with the implicit assumption that our
 * deepest function is the only caller of os.loadavg().
 */
var dtrace = spawn('dtrace', [ '-qwn', 'syscall::getloadavg:entry/pid == ' +
  process.pid + '/{ustack(100, 8192); exit(0); }' ]);

var output = '';

dtrace.stderr.on('data', function(data) {
  console.log('dtrace: ' + data);
});

dtrace.stdout.on('data', function(data) {
  output += data;
});

dtrace.on('exit', function(code) {
  if (code != 0) {
    console.error('dtrace exited with code ' + code);
    process.exit(code);
  }

  done = true;

  var sentinel = '(anon) as ';
  var lines = output.split('\n');

  for (var i = 0; i < lines.length; i++) {
    var line = lines[i];

    if (line.indexOf(sentinel) == -1 || frames.length === 0)
      continue;

    var frame = line.substr(line.indexOf(sentinel) + sentinel.length);
    var top = frames.shift();

    assert.equal(frame.indexOf(top), 0, 'unexpected frame where ' +
      top + ' was expected');
  }

  assert.equal(frames.length, 0, 'did not find expected frame ' + frames[0]);
  process.exit(0);
});

setTimeout(doogle, 10);
