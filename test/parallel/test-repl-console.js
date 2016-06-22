'use strict';
const common = require('../common');
const assert = require('assert');
const repl = require('repl');

// Create a dummy stream that does nothing
const stream = new common.ArrayStream();

var r = repl.start({
  input: stream,
  output: stream,
  useGlobal: false
});


// ensure that the repl context gets its own "console" instance
assert(r.context.console);

// ensure that the repl console instance is not the global one
assert.notStrictEqual(r.context.console, console);

// ensure that the repl console instance does not have a setter
assert.throws(() => r.context.console = 'foo', TypeError);
