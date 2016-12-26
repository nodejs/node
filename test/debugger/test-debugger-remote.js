'use strict';
const common = require('../common');
const assert = require('assert');
const spawn = require('child_process').spawn;
const path = require('path');

const PORT = common.PORT;
const scriptToDebug = path.join(common.fixturesDir, 'empty.js');

// running with debug agent
const child = spawn(process.execPath, [`--debug-brk=${PORT}`, scriptToDebug]);

// connect to debug agent
const interfacer = spawn(process.execPath, ['debug', `localhost:${PORT}`]);
interfacer.stdout.setEncoding('utf-8');

// fail the test if either of the processes exit normally
const debugBreakExit = common.fail.bind(null, 'child should not exit normally');
const debugExit = common.fail.bind(null, 'interfacer should not exit normally');
child.on('exit', debugBreakExit);
interfacer.on('exit', debugExit);

let buffer = '';
const expected = [
  `\bconnecting to localhost:${PORT} ... ok`,
  '\bbreak in test/fixtures/empty.js:2'
];
const actual = [];
interfacer.stdout.on('data', function(data) {
  data = (buffer + data).split('\n');
  buffer = data.pop();
  data.forEach(function(line) {
    interfacer.emit('line', line);
  });
});

interfacer.on('line', function(line) {
  line = line.replace(/^(debug> *)+/, '');
  if (expected.includes(line)) {
    actual.push(line);
  }
});

// allow time to start up the debugger
setTimeout(function() {
  // remove the exit handlers before killing the processes
  child.removeListener('exit', debugBreakExit);
  interfacer.removeListener('exit', debugExit);

  child.kill();
  interfacer.kill();
}, common.platformTimeout(2000));

process.on('exit', function() {
  // additional checks to ensure that both the processes were actually killed
  assert(child.killed);
  assert(interfacer.killed);
  assert.deepStrictEqual(actual, expected);
});

interfacer.stderr.pipe(process.stderr);
