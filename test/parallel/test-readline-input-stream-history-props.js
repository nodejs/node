'use strict';

// This tests that readline.createInterface(stream) does not read REPL history
// options from the input stream itself. A stream that happens to have an
// onHistoryFileLoaded property, for example a mock, used to start REPL history
// loading before the interface was set up and threw a TypeError.
// Refs: https://github.com/nodejs/node/issues/61526

const common = require('../common');
const assert = require('assert');
const readline = require('readline');
const { PassThrough } = require('stream');

{
  const input = new PassThrough();
  input.onHistoryFileLoaded = common.mustNotCall();
  const rl = readline.createInterface(input);
  assert.deepStrictEqual(rl.history, []);
  rl.close();
}

{
  const input = new PassThrough();
  input.onHistoryFileLoaded = common.mustNotCall();
  const rl = readline.createInterface({ input });
  rl.close();
}

// Properties of the stream with the names of history options are ignored.
{
  const input = new PassThrough();
  input.history = 'not an array';
  input.size = -1;
  const rl = readline.createInterface(input);
  assert.deepStrictEqual(rl.history, []);
  rl.close();
}

// History passed in an options object still works.
{
  const input = new PassThrough();
  const rl = readline.createInterface({ input, history: ['b', 'a'] });
  assert.deepStrictEqual(rl.history, ['b', 'a']);
  rl.close();
}
