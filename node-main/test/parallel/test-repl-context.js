'use strict';
require('../common');
const assert = require('assert');
const vm = require('vm');
const { startNewREPLServer } = require('../common/repl');

// Test context when useGlobal is false.
{
  const { replServer, output } = startNewREPLServer({
    terminal: false,
    useGlobal: false
  });

  // Ensure that the repl context gets its own "console" instance.
  assert(replServer.context.console);

  // Ensure that the repl console instance is not the global one.
  assert.notStrictEqual(replServer.context.console, console);
  assert.notStrictEqual(replServer.context.Object, Object);

  replServer.write('({} instanceof Object)\n');

  assert.strictEqual(output.accumulator, 'true\n');

  const context = replServer.createContext();
  // Ensure that the repl context gets its own "console" instance.
  assert(context.console instanceof require('console').Console);

  // Ensure that the repl's global property is the context.
  assert.strictEqual(context.global, context);

  // Ensure that the repl console instance is writable.
  context.console = 'foo';
  replServer.close();
}

// Test for context side effects.
{
  const { replServer } = startNewREPLServer({
    useGlobal: false
  });

  assert.ok(!replServer.underscoreAssigned);
  assert.strictEqual(replServer.lines.length, 0);

  // An assignment to '_' in the repl server
  replServer.write('_ = 500;\n');
  assert.ok(replServer.underscoreAssigned);
  assert.strictEqual(replServer.lines.length, 1);
  assert.strictEqual(replServer.lines[0], '_ = 500;');
  assert.strictEqual(replServer.last, 500);

  // Use the server to create a new context
  const context = replServer.createContext();

  // Ensure that creating a new context does not
  // have side effects on the server
  assert.ok(replServer.underscoreAssigned);
  assert.strictEqual(replServer.lines.length, 1);
  assert.strictEqual(replServer.lines[0], '_ = 500;');
  assert.strictEqual(replServer.last, 500);

  // Reset the server context
  replServer.resetContext();
  assert.ok(!replServer.underscoreAssigned);
  assert.strictEqual(replServer.lines.length, 0);

  // Ensure that assigning to '_' in the new context
  // does not change the value in our server.
  assert.ok(!replServer.underscoreAssigned);
  vm.runInContext('_ = 1000;\n', context);

  assert.ok(!replServer.underscoreAssigned);
  assert.strictEqual(replServer.lines.length, 0);
  replServer.close();
}
