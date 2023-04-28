'use strict';

require('../common');

const stream = require('stream');
const assert = require('assert');
const repl = require('repl');

let output = '';
const inputStream = new stream.PassThrough();
const outputStream = new stream.PassThrough();
outputStream.on('data', function(d) {
  output += d;
});

const r = repl.start({
  input: inputStream,
  output: outputStream,
  terminal: true
});

r.defineCommand('say1', {
  help: 'help for say1',
  action: function(thing) {
    output = '';
    this.output.write(`hello ${thing}\n`);
    this.displayPrompt();
  }
});

r.defineCommand('say2', function() {
  output = '';
  this.output.write('hello from say2\n');
  this.displayPrompt();
});

inputStream.write('.help\n');
assert.match(output, /\n\.say1 {5}help for say1\n/);
assert.match(output, /\n\.say2\n/);
inputStream.write('.say1 node developer\n');
assert.ok(output.startsWith('hello node developer\n'),
          `say1 output starts incorrectly: "${output}"`);
assert.ok(output.includes('> '),
          `say1 output does not include prompt: "${output}"`);
inputStream.write('.say2 node developer\n');
assert.ok(output.startsWith('hello from say2\n'),
          `say2 output starts incorrectly: "${output}"`);
assert.ok(output.includes('> '),
          `say2 output does not include prompt: "${output}"`);
