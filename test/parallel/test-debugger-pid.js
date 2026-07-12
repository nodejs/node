'use strict';
const common = require('../common');
common.skipIfInspectorDisabled();

if (common.isWindows)
  common.skip('unsupported function on windows');

const assert = require('assert');
const spawn = require('child_process').spawn;

let buffer = '';

// Connect to debug agent
const interfacer = spawn(process.execPath, ['inspect', '-p', '655555']);

interfacer.stdout.setEncoding('utf-8');
interfacer.stderr.setEncoding('utf-8');
const onData = (data) => {
  data = (buffer + data).split('\n');
  buffer = data.pop();
  data.forEach((line) => interfacer.emit('line', line));
};
interfacer.stdout.on('data', onData);
interfacer.stderr.on('data', onData);

interfacer.on('line', common.mustCall((line) => {
  assert.strictEqual(line, 'Target process: 655555 doesn\'t exist.');
}));
