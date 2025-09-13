'use strict';

const common = require('../common');
const repl = require('repl');
const stream = require('stream');
const assert = require('assert');

// Pasting big input should not cause the process to have a huge CPU usage.

const cpuUsage = process.cpuUsage();

const r = initRepl();
r.input.emit('data', 'a'.repeat(2e4) + '\n');
r.input.emit('data', '.exit\n');

r.once('exit', common.mustCall(() => {
  const diff = process.cpuUsage(cpuUsage);
  assert.ok(diff.user < 1e6);
}));

function initRepl() {
  const input = new stream();
  input.write = input.pause = input.resume = () => {};
  input.readable = true;

  const output = new stream();
  output.write = () => {};
  output.writable = true;

  return repl.start({
    input,
    output,
    terminal: true,
    prompt: ''
  });
}
