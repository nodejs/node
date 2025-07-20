'use strict';

require('../common');
const { startNewREPLServer } = require('../common/repl');

const stream = require('stream');
const assert = require('assert');

let output = '';
const outputStream = new stream.PassThrough();
outputStream.on('data', function(d) {
  output += d;
});

const { replServer: replServer, input } = startNewREPLServer({ prompt: '> ', terminal: true, output: outputStream });

replServer.defineCommand('say1', {
  help: 'help for say1',
  action: function(thing) {
    output = '';
    this.output.write(`hello ${thing}\n`);
    this.displayPrompt();
  }
});

replServer.defineCommand('say2', function() {
  output = '';
  this.output.write('hello from say2\n');
  this.displayPrompt();
});

input.run(['.help\n']);
assert.match(output, /\n\.say1 {5}help for say1\n/);
assert.match(output, /\n\.say2\n/);
input.run(['.say1 node developer\n']);
assert.ok(output.startsWith('hello node developer\n'),
          `say1 output starts incorrectly: "${output}"`);
assert.ok(output.includes('> '),
          `say1 output does not include prompt: "${output}"`);
input.run(['.say2 node developer\n']);
assert.ok(output.startsWith('hello from say2\n'),
          `say2 output starts incorrectly: "${output}"`);
assert.ok(output.includes('> '),
          `say2 output does not include prompt: "${output}"`);
