import { mustCall, mustNotCall } from '../common/index.mjs';
import { startNewREPLServer } from '../common/repl.js';
import assert from 'node:assert';

const defaultCommands =
  ['break', 'clear', 'editor', 'exit', 'help', 'load', 'save'];

// By default, and when `defineDefaultCommands` is explicitly `true`, the
// built-in commands are registered.
for (const defineDefaultCommands of [undefined, true]) {
  const { replServer } = startNewREPLServer({ defineDefaultCommands });
  assert.deepStrictEqual(
    Object.keys(replServer.commands).sort(),
    defaultCommands,
  );
  replServer.close();
}

// When `defineDefaultCommands` is `false`, no commands are registered.
{
  const { replServer, output, run } = startNewREPLServer({
    defineDefaultCommands: false,
  });
  assert.deepStrictEqual(Object.keys(replServer.commands), []);

  // The built-in commands are treated as unknown keywords rather than run.
  replServer.on('exit', mustNotCall('.exit should not close the REPL'));
  await run(['.exit\n']);
  assert.match(output.accumulator, /Invalid REPL keyword/);

  output.accumulator = '';
  await run(['.help\n']);
  assert.match(output.accumulator, /Invalid REPL keyword/);

  // Custom commands can still be defined, including ones that reuse a
  // built-in name.
  replServer.defineCommand('help', {
    help: 'custom help',
    action: mustCall(function() {
      this.output.write('custom help output\n');
      this.displayPrompt();
    }),
  });
  assert.deepStrictEqual(Object.keys(replServer.commands), ['help']);

  output.accumulator = '';
  await run(['.help\n']);
  assert.match(output.accumulator, /custom help output/);

  replServer.removeAllListeners('exit');
  replServer.close();
}
