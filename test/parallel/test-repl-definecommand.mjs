import '../common/index.mjs';
import { startNewREPLServer } from '../common/repl.js';
import assert from 'node:assert';

const { replServer, output, run } = startNewREPLServer({
  prompt: '> ',
  terminal: true,
});

replServer.defineCommand('say1', {
  help: 'help for say1',
  action: function(thing) {
    this.output.accumulator = '';
    this.output.write(`hello ${thing}\n`);
    this.displayPrompt();
  },
});

replServer.defineCommand('say2', function() {
  this.output.accumulator = '';
  this.output.write('hello from say2\n');
  this.displayPrompt();
});

await run(['.help\n']);
assert.match(output.accumulator, /\n\.say1 {5}help for say1\n/);
assert.match(output.accumulator, /\n\.say2\n/);

await run(['.say1 node developer\n']);
assert.ok(
  output.accumulator.startsWith('hello node developer\n'),
  `say1 output starts incorrectly: "${output.accumulator}"`,
);
assert.ok(
  output.accumulator.includes('> '),
  `say1 output does not include prompt: "${output.accumulator}"`,
);

await run(['.say2 node developer\n']);
assert.ok(
  output.accumulator.startsWith('hello from say2\n'),
  `say2 output starts incorrectly: "${output.accumulator}"`,
);
assert.ok(
  output.accumulator.includes('> '),
  `say2 output does not include prompt: "${output.accumulator}"`,
);

replServer.close();
