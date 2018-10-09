'use strict';
require('../common');
const ArrayStream = require('../common/arraystream');
const assert = require('assert');
const repl = require('repl');
const vm = require('vm');

// Create a dummy stream that does nothing.
const stream = new ArrayStream();

// Test context when useGlobal is false.
{
  const r = repl.start({
    input: stream,
    output: stream,
    useGlobal: false
  });

  // Ensure that the repl context gets its own "console" instance.
  assert(r.context.console);

  // Ensure that the repl console instance is not the global one.
  assert.notStrictEqual(r.context.console, console);

  const context = r.createContext();
  // Ensure that the repl context gets its own "console" instance.
  assert(context.console instanceof require('console').Console);

  // Ensure that the repl's global property is the context.
  assert.strictEqual(context.global, context);

  // Ensure that the repl console instance is writable.
  context.console = 'foo';
  r.close();
}

// Test for context side effects.
{
  const server = repl.start({ input: stream, output: stream });

  assert.ok(!server.underscoreAssigned);
  assert.strictEqual(server.lines.length, 0);

  // an assignment to '_' in the repl server
  server.write('_ = 500;\n');
  assert.ok(server.underscoreAssigned);
  assert.strictEqual(server.lines.length, 1);
  assert.strictEqual(server.lines[0], '_ = 500;');
  assert.strictEqual(server.last, 500);

  // use the server to create a new context
  const context = server.createContext();

  // ensure that creating a new context does not
  // have side effects on the server
  assert.ok(server.underscoreAssigned);
  assert.strictEqual(server.lines.length, 1);
  assert.strictEqual(server.lines[0], '_ = 500;');
  assert.strictEqual(server.last, 500);

  // reset the server context
  server.resetContext();
  assert.ok(!server.underscoreAssigned);
  assert.strictEqual(server.lines.length, 0);

  // ensure that assigning to '_' in the new context
  // does not change the value in our server.
  assert.ok(!server.underscoreAssigned);
  vm.runInContext('_ = 1000;\n', context);

  assert.ok(!server.underscoreAssigned);
  assert.strictEqual(server.lines.length, 0);
  server.close();
}
