'use strict';

const assert = require('assert');
const readline = require('readline');
const { PassThrough } = require('stream');

assert.doesNotThrow(() => {
  const input = new PassThrough();

  // This simulates the condition that previously caused
  // accidental REPL history initialization.
  input.onHistoryFileLoaded = () => {};

  readline.createInterface({
    input,
    output: new PassThrough()
  });
});
