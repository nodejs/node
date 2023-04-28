'use strict';

// Flags: --expose-internals

require('../common');
const stream = require('stream');
const REPL = require('internal/repl');
const assert = require('assert');
const inspect = require('util').inspect;
const { REPL_MODE_SLOPPY, REPL_MODE_STRICT } = require('repl');

const tests = [
  {
    env: {},
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
    env: { NODE_NO_READLINE: '0' },
    expected: { terminal: true, useColors: true }
  },
  {
    env: { NODE_REPL_MODE: 'sloppy' },
    expected: { terminal: true, useColors: true, replMode: REPL_MODE_SLOPPY }
  },
  {
    env: { NODE_REPL_MODE: 'strict' },
    expected: { terminal: true, useColors: true, replMode: REPL_MODE_STRICT }
  },
];

function run(test) {
  const env = test.env;
  const expected = test.expected;

  const opts = {
    terminal: true,
    input: new stream.Readable({ read() {} }),
    output: new stream.Writable({ write() {} })
  };

  Object.assign(process.env, env);

  REPL.createInternalRepl(process.env, opts, function(err, repl) {
    assert.ifError(err);

    assert.strictEqual(repl.terminal, expected.terminal,
                       `Expected ${inspect(expected)} with ${inspect(env)}`);
    assert.strictEqual(repl.useColors, expected.useColors,
                       `Expected ${inspect(expected)} with ${inspect(env)}`);
    assert.strictEqual(repl.replMode, expected.replMode || REPL_MODE_SLOPPY,
                       `Expected ${inspect(expected)} with ${inspect(env)}`);
    for (const key of Object.keys(env)) {
      delete process.env[key];
    }
    repl.close();
  });
}

tests.forEach(run);
