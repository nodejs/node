'use strict';
var common = require('../common');
var assert = require('assert');
var spawn = require('child_process').spawn;

var port = common.PORT + 1337;
var buffer = '';
var expected = [];
var scriptToDebug = common.fixturesDir + '/empty.js';

function fail() {
  assert(0); // `--debug-brk script.js` should not quit
}

// connect to debug agent
var interfacer = spawn(process.execPath, ['debug', '-p', '655555']);

console.error(process.execPath, 'debug', '-p', '655555');
interfacer.stdout.setEncoding('utf-8');
interfacer.stderr.setEncoding('utf-8');
var onData = function(data) {
  data = (buffer + data).split('\n');
  buffer = data.pop();
  data.forEach(function(line) {
    interfacer.emit('line', line);
  });
};
interfacer.stdout.on('data', onData);
interfacer.stderr.on('data', onData);

interfacer.on('line', function(line) {
  line = line.replace(/^(debug> *)+/, '');
  var expected = 'Target process: 655555 doesn\'t exist.';
  assert.ok(expected == line, 'Got unexpected line: ' + line);
});

interfacer.on('exit', function(code, signal) {
  assert.ok(code == 1, 'Got unexpected code: ' + code);
});
