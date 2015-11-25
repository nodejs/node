'use strict';
var common = require('../common'),
    assert = require('assert'),
    repl = require('repl');

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
