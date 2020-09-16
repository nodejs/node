'use strict';

const { invalidArgTypeHelper } = require('../common');
const assert = require('assert');
const { format, parseArgs } = require('util');

{
  const data = new Map([
    [
      { argv: ['--foo', '--bar'] },
      { options: { foo: true, bar: true }, positionals: [] }
    ],
    [
      { argv: ['--foo', '-b'] },
      { options: { foo: true, b: true }, positionals: [] }
    ],
    [
      { argv: ['---foo'] },
      { options: { foo: true }, positionals: [] }
    ],
    [
      { argv: ['--foo=bar'] },
      { options: { foo: true }, positionals: [] }
    ],
    [
      { argv: ['--foo=bar'], opts: { optionsWithValue: ['foo'] } },
      { options: { foo: 'bar' }, positionals: [] }
    ],
    [
      { argv: ['--foo', 'bar'] },
      { options: { foo: true }, positionals: ['bar'] }
    ],
    [
      { argv: ['--foo', 'bar'], opts: { optionsWithValue: 'foo' } },
      { options: { foo: 'bar' }, positionals: [] }
    ],
    [
      { argv: ['foo'] },
      { options: {}, positionals: ['foo'] }
    ],
    [
      { argv: ['--foo', '--', '--bar'] },
      { options: { foo: true }, positionals: ['--bar'] }
    ],
    [
      { argv: ['--foo=bar', '--foo=baz'], opts: { optionsWithValue: 'foo' } },
      { options: { foo: 'baz' }, positionals: [] }
    ],
    [
      {
        argv: ['--foo=bar', '--foo=baz'],
        opts: { optionsWithValue: 'foo', multiOptions: 'foo' }
      },
      { options: { foo: ['bar', 'baz'] }, positionals: [] }
    ],
    [
      { argv: ['--foo', '--foo'] },
      { options: { foo: true }, positionals: [] }
    ],
    [
      { argv: ['--foo', '--foo'], opts: { multiOptions: 'foo' } },
      { options: { foo: [true, true] }, positionals: [] }
    ],
    [
      { argv: ['--foo=true'] },
      { options: { foo: true }, positionals: [] }
    ],
    [
      { argv: ['--foo=false'] },
      { options: { foo: false }, positionals: [] }
    ],
    [
      { argv: ['--foo=bar'] },
      { options: { foo: true }, positionals: [] }
    ],
    [
      { argv: ['--foo', 'true'] },
      { options: { foo: true }, positionals: ['true'] }
    ],
    [
      { argv: ['--foo', 'false'] },
      { options: { foo: true }, positionals: ['false'] }
    ],
    [
      { argv: ['--foo', 'true'], opts: { optionsWithValue: ['foo'] } },
      { options: { foo: 'true' }, positionals: [] }
    ],
    [
      {
        argv: ['--foo', 'false', 'false'],
        opts: { optionsWithValue: ['foo'] }
      },
      { options: { foo: 'false' }, positionals: ['false'] }
    ],
    [
      { argv: ['--foo=true', '--foo=false'] },
      { options: { foo: false }, positionals: [] }
    ],
    [
      { argv: ['--foo', 'true', '--foo', 'false'] },
      { options: { foo: true }, positionals: ['true', 'false'] }
    ],
    [
      { argv: ['--foo', 'true', '--foo', 'false'],
        opts: { optionsWithValue: 'foo', multiOptions: ['foo'] } },
      { options: { foo: ['true', 'false'] }, positionals: [] }
    ],
    [
      { argv: ['--foo', 'true', '--foo', 'false'],
        opts: { optionsWithValue: 'foo' } },
      { options: { foo: 'false' }, positionals: [] }
    ],
    [
      { argv: ['--foo', 'true', '--foo=false'],
        opts: { optionsWithValue: 'foo', multiOptions: ['foo'] } },
      { options: { foo: ['true', 'false'] }, positionals: [] }
    ],
    [
      { argv: ['--foo', 'true', '--foo=false'] },
      { options: { foo: false }, positionals: ['true'] }
    ],
    [
      {
        argv: ['--foo', 'bar', '--foo', 'baz'],
        opts: { optionsWithValue: ['foo'], multiOptions: 'foo' }
      },
      { options: { foo: ['bar', 'baz'] }, positionals: [] }
    ],
    [
      { argv: ['--foo', '--bar'], opts: { optionsWithValue: 'foo' } },
      { options: { foo: true, bar: true }, positionals: [] }
    ],
    [
      { argv: ['--foo=--bar'], opts: { optionsWithValue: 'foo' } },
      { options: { foo: '--bar' }, positionals: [] }
    ],
    [
      { argv: ['-_'] },
      { options: { _: true }, positionals: [] }
    ],
    [
      { argv: ['--_=bar'], opts: { optionsWithValue: '_' } },
      { options: { _: 'bar' }, positionals: [] }
    ],
    [
      { argv: ['--_', 'bar'], opts: { optionsWithValue: '_' } },
      { options: { _: 'bar' }, positionals: [] }
    ],
    [
      { argv: [], opts: { optionsWithValue: 'foo', multiOptions: 'foo' } },
      { options: { }, positionals: [] }
    ],
    [
      { argv: [], opts: { multiOptions: 'foo' } },
      { options: { }, positionals: [] }
    ],
    [
      { argv: ['--foo'], opts: { optionsWithValue: 'foo' } },
      { options: { foo: '' }, positionals: [] }
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
    assert.deepStrictEqual(
      parseArgs(),
      { options: { foo: true }, positionals: ['bar'] }
    );
    assert.deepStrictEqual(
      parseArgs({ optionsWithValue: 'foo' }),
      { options: { foo: 'bar' }, positionals: [] }
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
