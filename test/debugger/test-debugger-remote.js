'use strict';
var common = require('../common');
var assert = require('assert');
var spawn = require('child_process').spawn;

var buffer = '';
var scriptToDebug = common.fixturesDir + '/empty.js';

function fail() {
  assert(0); // `--debug-brk script.js` should not quit
}

// running with debug agent
var child = spawn(process.execPath, ['--debug-brk=5959', scriptToDebug]);

console.error(process.execPath, '--debug-brk=5959', scriptToDebug);

// connect to debug agent
var interfacer = spawn(process.execPath, ['debug', 'localhost:5959']);

console.error(process.execPath, 'debug', 'localhost:5959');
interfacer.stdout.setEncoding('utf-8');
interfacer.stdout.on('data', function(data) {
  data = (buffer + data).split('\n');
  buffer = data.pop();
  data.forEach(function(line) {
    interfacer.emit('line', line);
  });
});

interfacer.on('line', function(line) {
  line = line.replace(/^(debug> *)+/, '');
  console.log(line);
  var expected = '\bconnecting to localhost:5959 ... ok';
  assert.ok(expected == line, 'Got unexpected line: ' + line);
});

// allow time to start up the debugger
setTimeout(function() {
  child.removeListener('exit', fail);
  child.kill();
  interfacer.removeListener('exit', fail);
  interfacer.kill();
}, 2000);

process.on('exit', function() {
  assert(child.killed);
  assert(interfacer.killed);
});

interfacer.stderr.pipe(process.stderr);
