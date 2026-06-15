'use strict';
const common = require('../common');
const assert = require('assert');
const { startNewREPLServer } = require('../common/repl');


const dotCommandSyntaxError =
  /Uncaught SyntaxError: Unexpected token '\.'/;


function runDotCommand(command, validate) {
  const { replServer, output } = startNewREPLServer();

  replServer.on('exit', common.mustCall());
  replServer.write('function a() {\n');
  replServer.write('console.log("logging");\n');
  replServer.write(`${command}\n`);
  validate(replServer, output);
  replServer.write('arr = [1,\n');
  replServer.write('console.log("logging");\n');
  replServer.write(`${command}\n`);
  validate(replServer, output);
  replServer.close();
}

runDotCommand('.break', common.mustCall((replServer, output) => {
  replServer.write('1 + 1\n');
  assert.doesNotMatch(output.accumulator, dotCommandSyntaxError);
  assert.match(output.accumulator, /2\n/);
}, 2));

runDotCommand('.clear', common.mustCall((replServer, output) => {
  replServer.write('1 + 1\n');
  assert.doesNotMatch(output.accumulator, dotCommandSyntaxError);
  assert.match(output.accumulator, /2\n/);
}, 2));

runDotCommand('.help', common.mustCall((replServer, output) => {
  replServer.write('1 + 1\n');
  assert.doesNotMatch(output.accumulator, dotCommandSyntaxError);
  assert.match(output.accumulator, /2\n/);
}, 2));
