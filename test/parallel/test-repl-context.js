'use strict';
const common = require('../common');
const assert = require('assert');
const repl = require('repl');

// Create a dummy stream that does nothing
const stream = new common.ArrayStream();

// Test when useGlobal is false
testContext(repl.start({
  input: stream,
  output: stream,
  useGlobal: false
}));

function testContext(repl) {
  const context = repl.createContext();
  // ensure that the repl context gets its own "console" instance
  assert(context.console instanceof require('console').Console);

  // ensure that the repl's global property is the context
  assert.strictEqual(context.global, context);

  // ensure that the repl console instance does not have a setter
  assert.throws(() => context.console = 'foo');
}
