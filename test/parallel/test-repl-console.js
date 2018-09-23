'use strict';
var common = require('../common'),
    assert = require('assert'),
    Stream = require('stream'),
    repl = require('repl');

// create a dummy stream that does nothing
var stream = new Stream();
stream.write = stream.pause = stream.resume = function() {};
stream.readable = stream.writable = true;

var r = repl.start({
  input: stream,
  output: stream,
  useGlobal: false
});


// ensure that the repl context gets its own "console" instance
assert(r.context.console);

// ensure that the repl console instance is not the global one
assert.notStrictEqual(r.context.console, console);
