'use strict';
const common = require('../common');
const assert = require('assert');
const { startNewREPLServer } = require('../common/repl');

const input = ['const foo = {', '};', 'foo'];
const dotCommandSyntaxError =
  /Uncaught SyntaxError: Unexpected token '\.'/;

function run({ useColors }) {
  const { replServer, output } = startNewREPLServer({ useColors });

  replServer.on('exit', common.mustCall(() => {
    const actual = output.accumulator.split('\n');

    // Validate the output, which contains terminal escape codes.
    assert.strictEqual(actual.length, 6);
    assert.ok(actual[0].endsWith(input[0]));
    assert.ok(actual[1].includes('| '));
    assert.ok(actual[1].endsWith(input[1]));
    assert.ok(actual[2].includes('undefined'));
    assert.ok(actual[3].endsWith(input[2]));
    assert.strictEqual(actual[4], '{}');
  }));

  input.forEach((line) => replServer.write(`${line}\n`));
  replServer.close();
}

run({ useColors: true });
run({ useColors: false });

function runDotCommandAfterRecoverable(command, validate) {
  const { replServer, output } = startNewREPLServer();

  replServer.on('exit', common.mustCall());
  replServer.write('function a() {\n');
  replServer.write(`${command}\n`);

  assert.doesNotMatch(output.accumulator, dotCommandSyntaxError);
  validate(replServer, output);

  replServer.close();
}

runDotCommandAfterRecoverable('.break', (replServer, output) => {
  replServer.write('1 + 1\n');
  assert.doesNotMatch(output.accumulator, dotCommandSyntaxError);
  assert.match(output.accumulator, /2\n/);
});

runDotCommandAfterRecoverable('.help', (replServer, output) => {
  assert.match(output.accumulator, /\.break\s+Sometimes you get stuck/);
  assert.match(output.accumulator, /\.help\s+Print this help message/);

  replServer.write('.break\n');
  replServer.write('1 + 1\n');

  assert.doesNotMatch(output.accumulator, dotCommandSyntaxError);
  assert.match(output.accumulator, /2\n/);
});

function runBufferedDotCommandAfterRecoverable(command, validate) {
  const { replServer, input, output } =
    startNewREPLServer({ terminal: false });

  replServer.on('exit', common.mustCall());
  input.run(['function a() {', command]);

  assert.doesNotMatch(output.accumulator, dotCommandSyntaxError);
  validate(input, output);

  replServer.close();
}

runBufferedDotCommandAfterRecoverable('.break', (input, output) => {
  input.run(['1 + 1']);
  assert.doesNotMatch(output.accumulator, dotCommandSyntaxError);
  assert.match(output.accumulator, /2\n/);
});

runBufferedDotCommandAfterRecoverable('.help', (input, output) => {
  assert.match(output.accumulator, /\.break\s+Sometimes you get stuck/);
  assert.match(output.accumulator, /\.help\s+Print this help message/);

  input.run(['.break', '1 + 1']);

  assert.doesNotMatch(output.accumulator, dotCommandSyntaxError);
  assert.match(output.accumulator, /2\n/);
});

{
  const { replServer, output } = startNewREPLServer();
  let exited = false;

  replServer.on('exit', common.mustCall(() => {
    exited = true;
  }));
  replServer.write('function a() {\n');
  replServer.write('.exit\n');

  assert.strictEqual(exited, true);
  assert.doesNotMatch(output.accumulator, dotCommandSyntaxError);
}

{
  const { replServer, input, output } =
    startNewREPLServer({ terminal: false });
  let exited = false;

  replServer.on('exit', common.mustCall(() => {
    exited = true;
  }));
  input.run(['function a() {', '.exit']);

  assert.strictEqual(exited, true);
  assert.doesNotMatch(output.accumulator, dotCommandSyntaxError);
}
