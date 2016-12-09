'use strict';

require('../common');

const stream = require('stream');
const assert = require('assert');
const repl = require('repl');

var output = '';
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
    this.write('hello ' + thing);
    this.displayPrompt();
  }
});

r.defineCommand('say2', function() {
  output = '';
  this.write('hello from say2');
  this.displayPrompt();
});

inputStream.write('.help\n');
assert(/\n.say1     help for say1\n/.test(output), 'help for say1 not present');
assert(/\n.say2\n/.test(output), 'help for say2 not present');
inputStream.write('.say1 node developer\n');
assert(/> hello node developer/.test(output), 'say1 outputted incorrectly');
inputStream.write('.say2 node developer\n');
assert(/> hello from say2/.test(output), 'say2 outputted incorrectly');
