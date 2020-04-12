'use strict';
const common = require('../common');
common.skipIfInspectorDisabled();
const assert = require('assert');
const spawn = require('child_process').spawn;

let buffer = '';

// Connect to debug agent
const interfacer = spawn(process.execPath, ['debug', '-p', '655555']);

interfacer.stdout.setEncoding('utf-8');
interfacer.stderr.setEncoding('utf-8');
const onData = (data) => {
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
  switch (++lineCount) {
    case 1:
      expected =
        new RegExp(`^\\(node:${pid}\\) \\[DEP0068\\] DeprecationWarning: `);
      assert.match(line, expected);
      break;
    case 2:
      assert.match(line, /Use `node --trace-deprecation \.\.\.` to show where /);
      break;
    case 3:
      // Doesn't currently work on Windows.
      if (!common.isWindows) {
        expected = "Target process: 655555 doesn't exist.";
        assert.strictEqual(line, expected);
      }
      break;

    default:
      if (!common.isWindows)
        assert.fail(`unexpected line received: ${line}`);
  }
});

interfacer.on('exit', function(code, signal) {
  assert.strictEqual(code, 1, `Got unexpected code: ${code}`);
  if (!common.isWindows) {
    assert.strictEqual(lineCount, 3);
  }
});
