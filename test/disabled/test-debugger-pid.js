'use strict';
const common = require('../common');
const assert = require('assert');
const spawn = require('child_process').spawn;

let buffer = '';

// connect to debug agent
const interfacer = spawn(process.execPath, ['debug', '-p', '655555']);

console.error(process.execPath, 'debug', '-p', '655555');
interfacer.stdout.setEncoding('utf-8');
interfacer.stderr.setEncoding('utf-8');
const onData = function(data) {
  data = (buffer + data).split('\n');
  buffer = data.pop();
  data.forEach(function(line) {
    interfacer.emit('line', line);
  });
};
interfacer.stdout.on('data', onData);
interfacer.stderr.on('data', onData);

let lineCount = 0;
interfacer.on('line', function(line) {
  let expected;
  const pid = interfacer.pid;
  if (common.isWindows) {
    switch (++lineCount) {
      case 1:
        line = line.replace(/^(debug> *)+/, '');
        const msg = 'There was an internal error in Node\'s debugger. ' +
                    'Please report this bug.';
        expected = `(node:${pid}) ${msg}`;
        break;

      default:
        return;
    }
  } else {
    line = line.replace(/^(debug> *)+/, '');
    expected = `(node:${pid}) Target process: 655555 doesn't exist.`;
  }

  assert.strictEqual(expected, line);
});

interfacer.on('exit', function(code, signal) {
  assert.strictEqual(code, 1, `Got unexpected code: ${code}`);
});
