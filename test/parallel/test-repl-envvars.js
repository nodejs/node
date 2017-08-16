'use strict';

// Flags: --expose-internals

const common = require('../common');
const stream = require('stream');
const REPL = require('internal/repl');
const assert = require('assert');
const inspect = require('util').inspect;

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
  }
];

function run(test) {
  const env = test.env;
  const expected = test.expected;
  const opts = {
    terminal: true,
    input: new stream.Readable({ read() {} }),
    output: new stream.Writable({ write() {} })
  };

  REPL.createInternalRepl(env, opts, function(err, repl) {
    assert.ifError(err);

    // The REPL registers 'module' and 'require' globals
    common.allowGlobals(repl.context.module, repl.context.require);

    assert.strictEqual(expected.terminal, repl.terminal,
                       `Expected ${inspect(expected)} with ${inspect(env)}`);
    assert.strictEqual(expected.useColors, repl.useColors,
                       `Expected ${inspect(expected)} with ${inspect(env)}`);
    repl.close();
  });
}

tests.forEach(run);
