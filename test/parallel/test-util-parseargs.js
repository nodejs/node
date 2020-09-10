'use strict';

const { invalidArgTypeHelper } = require('../common');
const assert = require('assert');
const { format, parseArgs } = require('util');

{
  const data = new Map([
    [
      { argv: ['--foo', '--bar'] },
      { foo: true, bar: true, _: [] }
    ],
    [
      { argv: ['--foo', '-b'] },
      { foo: true, b: true, _: [] }
    ],
    [
      { argv: ['---foo'] },
      { foo: true, _: [] }
    ],
    [
      { argv: ['--foo=bar'] },
      { foo: true, _: [] }
    ],
    [
      { argv: ['--foo=bar'], opts: { expectsValue: ['foo'] } },
      { foo: 'bar', _: [] }
    ],
    [
      { argv: ['--foo', 'bar'] },
      { foo: true, _: ['bar'] }
    ],
    [
      { argv: ['--foo', 'bar'], opts: { expectsValue: 'foo' } },
      { foo: 'bar', _: [] }
    ],
    [
      { argv: ['foo'] },
      { _: ['foo'] }
    ],
    [
      { argv: ['--foo', '--', '--bar'] },
      { foo: true, _: ['--bar'] }
    ],
    [
      { argv: ['--foo=bar', '--foo=baz'], opts: { expectsValue: 'foo' } },
      { foo: ['bar', 'baz'], _: [] }
    ],
    [
      { argv: ['--foo', '--foo'] },
      { foo: 2, _: [] }
    ],
    [
      { argv: ['--foo=true'] },
      { foo: true, _: [] }
    ],
    [
      { argv: ['--foo=false'] },
      { foo: true, _: [] }
    ],
    [
      { argv: ['--foo', 'true'] },
      { foo: true, _: ['true'] }
    ],
    [
      { argv: ['--foo', 'true'], opts: { expectsValue: ['foo'] } },
      { foo: 'true', _: [] }
    ],
    [
      { argv: ['--foo', 'false', 'false'], opts: { expectsValue: ['foo'] } },
      { foo: 'false', _: ['false'] }
    ],
    [
      { argv: ['--foo=true', '--foo=false'] },
      { foo: 2, _: [] }
    ],
    [
      { argv: ['--foo', 'true', '--foo', 'false'] },
      { foo: 2, _: ['true', 'false'] }
    ],
    [
      { argv: ['--foo', 'true', '--foo', 'false'],
        opts: { expectsValue: 'foo' } },
      { foo: ['true', 'false'], _: [] }
    ],
    [
      { argv: ['--foo', 'true', '--foo=false'],
        opts: { expectsValue: 'foo' } },
      { foo: ['true', 'false'], _: [] }
    ],
    [
      { argv: ['--foo', 'true', '--foo=false'] },
      { foo: 2, _: ['true'] }
    ],
    [
      {
        argv: ['--foo', 'bar', '--foo', 'baz'],
        opts: { expectsValue: ['foo'] }
      },
      { foo: ['bar', 'baz'], _: [] }
    ],
    [
      { argv: ['--foo', '--bar'], opts: { expectsValue: 'foo' } },
      { foo: true, bar: true, _: [] }
    ],
    [
      { argv: ['--foo=--bar'], opts: { expectsValue: 'foo' } },
      { foo: '--bar', _: [] }
    ],
    [
      { argv: ['-_'] },
      { _: [] }
    ],
    [
      { argv: ['--_=bar'], opts: { expectsValue: '_' } },
      { _: [] }
    ],
    [
      { argv: ['--_', 'bar'], opts: { expectsValue: '_' } },
      { _: [] }
    ]
  ]);

  data.forEach((output, input) => {
    const { argv, opts } = input;
    assert.deepStrictEqual(
      parseArgs(argv, opts),
      output,
      format('%o does not parse to %o', input, output)
    );
  });
}

{
  // Use of `process.argv.slice(2)` as default `argv` value
  const originalArgv = process.argv;
  process.argv = [process.execPath, __filename, '--foo', 'bar'];
  try {
    assert.deepStrictEqual(parseArgs(), { foo: true, _: ['bar'] });
    assert.deepStrictEqual(
      parseArgs({ expectsValue: 'foo' }), { foo: 'bar', _: [] }
    );
  } finally {
    process.argv = originalArgv;
  }
}

{
  // error checking
  const value = 'strings not allowed';
  assert.throws(() => {
    parseArgs(value);
  }, {
    code: 'ERR_INVALID_ARG_TYPE',
    name: 'TypeError',
    message:
      'The "options" argument must be of type object.' +
      invalidArgTypeHelper(value)
  });
}
