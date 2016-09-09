'use strict';

const common = require('../common');
const assert = require('assert');
const path = require('path');
const repl = require('repl');

process.on('exit', () => {
  assert.strictEqual(/var foo bar;/.test(error), true);
});

let error = '';
common.ArrayStream.prototype.write = (output) => {
  error += output;
};

const runAsHuman = (cmd, repl) => {
  const cmds = Array.isArray(cmd) ? cmd : [cmd];
  repl.input.run(cmds);
  repl._sawKeyPress = true;
  repl.input.emit('keypress', null, { name: 'return' });
};

const clear = (repl) => {
  repl.input.emit('keypress', null, { name: 'c', ctrl: true});
  runAsHuman('.clear', repl);
};

const putIn = new common.ArrayStream();
const replServer = repl.start({
  input: putIn,
  output: putIn,
  terminal: true
});
let file = path.join(common.fixturesDir, 'syntax', 'bad_syntax');

if (common.isWindows)
  file = file.replace(/\\/g, '\\\\');

clear(replServer);
runAsHuman(`require('${file}');`, replServer);
