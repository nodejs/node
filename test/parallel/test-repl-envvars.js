'use strict';

// Flags: --expose-internals

require('../common');
const stream = require('stream');
const REPL = require('internal/repl');
const assert = require('assert');
const debug = require('util').debuglog('test');

const tests = [
  {
    env: { TERM: 'xterm-256' },
    expected: { terminal: true, useColors: true }
  },
  {
    env: { NODE_DISABLE_COLORS: '1' },
    expected: { terminal: true, useColors: false }
  },
  {
    env: { NODE_NO_READLINE: '1' },
    expected: { terminal: false, useColors: false }
  },
  {
    env: { TERM: 'dumb' },
    expected: { terminal: true, useColors: false }
  },
  {
    env: { NODE_NO_READLINE: '1', NODE_DISABLE_COLORS: '1' },
    expected: { terminal: false, useColors: false }
  },
  {
    env: { NODE_NO_READLINE: '0', TERM: 'xterm-256' },
    expected: { terminal: true, useColors: true }
  }
];

const originalEnv = process.env;

function run(test) {
  const env = test.env;
  const expected = test.expected;

  process.env = { ...originalEnv, ...env };

  const opts = {
    terminal: true,
    input: new stream.Readable({ read() {} }),
    output: new stream.Writable({ write() {} })
  };

  REPL.createInternalRepl(opts, (repl) => {
    debug(env);
    const { terminal, useColors } = repl;
    assert.deepStrictEqual({ terminal, useColors }, expected);
    repl.close();
    if (tests.length)
      run(tests.shift());
  });
}

run(tests.shift());
