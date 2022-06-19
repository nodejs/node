import '../common/index.mjs';
import assert from 'node:assert';
import { test } from 'node:test';
import { parseArgs } from 'node:util';

test('when short option used as flag then stored as flag', () => {
  const args = ['-f'];
  const expected = { values: { __proto__: null, f: true }, positionals: [] };
  const result = parseArgs({ strict: false, args });
  assert.deepStrictEqual(result, expected);
});

test('when short option used as flag before positional then stored as flag and positional (and not value)', () => {
  const args = ['-f', 'bar'];
  const expected = { values: { __proto__: null, f: true }, positionals: [ 'bar' ] };
  const result = parseArgs({ strict: false, args });
  assert.deepStrictEqual(result, expected);
});

test('when short option `type: "string"` used with value then stored as value', () => {
  const args = ['-f', 'bar'];
  const options = { f: { type: 'string' } };
  const expected = { values: { __proto__: null, f: 'bar' }, positionals: [] };
  const result = parseArgs({ args, options });
  assert.deepStrictEqual(result, expected);
});

test('when short option listed in short used as flag then long option stored as flag', () => {
  const args = ['-f'];
  const options = { foo: { short: 'f', type: 'boolean' } };
  const expected = { values: { __proto__: null, foo: true }, positionals: [] };
  const result = parseArgs({ args, options });
  assert.deepStrictEqual(result, expected);
});

test('when short option listed in short and long listed in `type: "string"` and ' +
     'used with value then long option stored as value', () => {
  const args = ['-f', 'bar'];
  const options = { foo: { short: 'f', type: 'string' } };
  const expected = { values: { __proto__: null, foo: 'bar' }, positionals: [] };
  const result = parseArgs({ args, options });
  assert.deepStrictEqual(result, expected);
});

test('when short option `type: "string"` used without value then stored as flag', () => {
  const args = ['-f'];
  const options = { f: { type: 'string' } };
  const expected = { values: { __proto__: null, f: true }, positionals: [] };
  const result = parseArgs({ strict: false, args, options });
  assert.deepStrictEqual(result, expected);
});

test('short option group behaves like multiple short options', () => {
  const args = ['-rf'];
  const options = { };
  const expected = { values: { __proto__: null, r: true, f: true }, positionals: [] };
  const result = parseArgs({ strict: false, args, options });
  assert.deepStrictEqual(result, expected);
});

test('short option group does not consume subsequent positional', () => {
  const args = ['-rf', 'foo'];
  const options = { };
  const expected = { values: { __proto__: null, r: true, f: true }, positionals: ['foo'] };
  const result = parseArgs({ strict: false, args, options });
  assert.deepStrictEqual(result, expected);
});

// See: Guideline 5 https://pubs.opengroup.org/onlinepubs/9699919799/basedefs/V1_chap12.html
test('if terminal of short-option group configured `type: "string"`, subsequent positional is stored', () => {
  const args = ['-rvf', 'foo'];
  const options = { f: { type: 'string' } };
  const expected = { values: { __proto__: null, r: true, v: true, f: 'foo' }, positionals: [] };
  const result = parseArgs({ strict: false, args, options });
  assert.deepStrictEqual(result, expected);
});

test('handles short-option groups in conjunction with long-options', () => {
  const args = ['-rf', '--foo', 'foo'];
  const options = { foo: { type: 'string' } };
  const expected = { values: { __proto__: null, r: true, f: true, foo: 'foo' }, positionals: [] };
  const result = parseArgs({ strict: false, args, options });
  assert.deepStrictEqual(result, expected);
});

test('handles short-option groups with "short" alias configured', () => {
  const args = ['-rf'];
  const options = { remove: { short: 'r', type: 'boolean' } };
  const expected = { values: { __proto__: null, remove: true, f: true }, positionals: [] };
  const result = parseArgs({ strict: false, args, options });
  assert.deepStrictEqual(result, expected);
});

test('handles short-option followed by its value', () => {
  const args = ['-fFILE'];
  const options = { foo: { short: 'f', type: 'string' } };
  const expected = { values: { __proto__: null, foo: 'FILE' }, positionals: [] };
  const result = parseArgs({ strict: false, args, options });
  assert.deepStrictEqual(result, expected);
});

test('Everything after a bare `--` is considered a positional argument', () => {
  const args = ['--', 'barepositionals', 'mopositionals'];
  const expected = { values: { __proto__: null }, positionals: ['barepositionals', 'mopositionals'] };
  const result = parseArgs({ allowPositionals: true, args });
  assert.deepStrictEqual(result, expected, Error('testing bare positionals'));
});

test('args are true', () => {
  const args = ['--foo', '--bar'];
  const expected = { values: { __proto__: null, foo: true, bar: true }, positionals: [] };
  const result = parseArgs({ strict: false, args });
  assert.deepStrictEqual(result, expected, Error('args are true'));
});

test('arg is true and positional is identified', () => {
  const args = ['--foo=a', '--foo', 'b'];
  const expected = { values: { __proto__: null, foo: true }, positionals: ['b'] };
  const result = parseArgs({ strict: false, args });
  assert.deepStrictEqual(result, expected, Error('arg is true and positional is identified'));
});

test('args equals are passed `type: "string"`', () => {
  const args = ['--so=wat'];
  const options = { so: { type: 'string' } };
  const expected = { values: { __proto__: null, so: 'wat' }, positionals: [] };
  const result = parseArgs({ args, options });
  assert.deepStrictEqual(result, expected, Error('arg value is passed'));
});

test('when args include single dash then result stores dash as positional', () => {
  const args = ['-'];
  const expected = { values: { __proto__: null }, positionals: ['-'] };
  const result = parseArgs({ allowPositionals: true, args });
  assert.deepStrictEqual(result, expected);
});

test('zero config args equals are parsed as if `type: "string"`', () => {
  const args = ['--so=wat'];
  const options = { };
  const expected = { values: { __proto__: null, so: 'wat' }, positionals: [] };
  const result = parseArgs({ strict: false, args, options });
  assert.deepStrictEqual(result, expected, Error('arg value is passed'));
});

test('same arg is passed twice `type: "string"` and last value is recorded', () => {
  const args = ['--foo=a', '--foo', 'b'];
  const options = { foo: { type: 'string' } };
  const expected = { values: { __proto__: null, foo: 'b' }, positionals: [] };
  const result = parseArgs({ args, options });
  assert.deepStrictEqual(result, expected, Error('last arg value is passed'));
});

test('args equals pass string including more equals', () => {
  const args = ['--so=wat=bing'];
  const options = { so: { type: 'string' } };
  const expected = { values: { __proto__: null, so: 'wat=bing' }, positionals: [] };
  const result = parseArgs({ args, options });
  assert.deepStrictEqual(result, expected, Error('arg value is passed'));
});

test('first arg passed for `type: "string"` and "multiple" is in array', () => {
  const args = ['--foo=a'];
  const options = { foo: { type: 'string', multiple: true } };
  const expected = { values: { __proto__: null, foo: ['a'] }, positionals: [] };
  const result = parseArgs({ args, options });
  assert.deepStrictEqual(result, expected, Error('first multiple in array'));
});

test('args are passed `type: "string"` and "multiple"', () => {
  const args = ['--foo=a', '--foo', 'b'];
  const options = {
    foo: {
      type: 'string',
      multiple: true,
    },
  };
  const expected = { values: { __proto__: null, foo: ['a', 'b'] }, positionals: [] };
  const result = parseArgs({ args, options });
  assert.deepStrictEqual(result, expected, Error('both arg values are passed'));
});

test('when expecting `multiple:true` boolean option and option used multiple times then result includes array of ' +
     'booleans matching usage', () => {
  const args = ['--foo', '--foo'];
  const options = {
    foo: {
      type: 'boolean',
      multiple: true,
    },
  };
  const expected = { values: { __proto__: null, foo: [true, true] }, positionals: [] };
  const result = parseArgs({ args, options });
  assert.deepStrictEqual(result, expected);
});

test('order of option and positional does not matter (per README)', () => {
  const args1 = ['--foo=bar', 'baz'];
  const args2 = ['baz', '--foo=bar'];
  const options = { foo: { type: 'string' } };
  const expected = { values: { __proto__: null, foo: 'bar' }, positionals: ['baz'] };
  assert.deepStrictEqual(
    parseArgs({ allowPositionals: true, args: args1, options }),
    expected,
    Error('option then positional')
  );
  assert.deepStrictEqual(
    parseArgs({ allowPositionals: true, args: args2, options }),
    expected,
    Error('positional then option')
  );
});

test('correct default args when use node -p', () => {
  const holdArgv = process.argv;
  process.argv = [process.argv0, '--foo'];
  const holdExecArgv = process.execArgv;
  process.execArgv = ['-p', '0'];
  const result = parseArgs({ strict: false });

  const expected = { values: { __proto__: null, foo: true },
                     positionals: [] };
  assert.deepStrictEqual(result, expected);
  process.argv = holdArgv;
  process.execArgv = holdExecArgv;
});

test('correct default args when use node --print', () => {
  const holdArgv = process.argv;
  process.argv = [process.argv0, '--foo'];
  const holdExecArgv = process.execArgv;
  process.execArgv = ['--print', '0'];
  const result = parseArgs({ strict: false });

  const expected = { values: { __proto__: null, foo: true },
                     positionals: [] };
  assert.deepStrictEqual(result, expected);
  process.argv = holdArgv;
  process.execArgv = holdExecArgv;
});

test('correct default args when use node -e', () => {
  const holdArgv = process.argv;
  process.argv = [process.argv0, '--foo'];
  const holdExecArgv = process.execArgv;
  process.execArgv = ['-e', '0'];
  const result = parseArgs({ strict: false });

  const expected = { values: { __proto__: null, foo: true },
                     positionals: [] };
  assert.deepStrictEqual(result, expected);
  process.argv = holdArgv;
  process.execArgv = holdExecArgv;
});

test('correct default args when use node --eval', () => {
  const holdArgv = process.argv;
  process.argv = [process.argv0, '--foo'];
  const holdExecArgv = process.execArgv;
  process.execArgv = ['--eval', '0'];
  const result = parseArgs({ strict: false });
  const expected = { values: { __proto__: null, foo: true },
                     positionals: [] };
  assert.deepStrictEqual(result, expected);
  process.argv = holdArgv;
  process.execArgv = holdExecArgv;
});

test('correct default args when normal arguments', () => {
  const holdArgv = process.argv;
  process.argv = [process.argv0, 'script.js', '--foo'];
  const holdExecArgv = process.execArgv;
  process.execArgv = [];
  const result = parseArgs({ strict: false });

  const expected = { values: { __proto__: null, foo: true },
                     positionals: [] };
  assert.deepStrictEqual(result, expected);
  process.argv = holdArgv;
  process.execArgv = holdExecArgv;
});

test('excess leading dashes on options are retained', () => {
  // Enforce a design decision for an edge case.
  const args = ['---triple'];
  const options = { };
  const expected = {
    values: { '__proto__': null, '-triple': true },
    positionals: []
  };
  const result = parseArgs({ strict: false, args, options });
  assert.deepStrictEqual(result, expected, Error('excess option dashes are retained'));
});

test('positional arguments are allowed by default in strict:false', () => {
  const args = ['foo'];
  const options = { };
  const expected = {
    values: { __proto__: null },
    positionals: ['foo']
  };
  const result = parseArgs({ strict: false, args, options });
  assert.deepStrictEqual(result, expected);
});

test('positional arguments may be explicitly disallowed in strict:false', () => {
  const args = ['foo'];
  const options = { };
  assert.throws(() => { parseArgs({ strict: false, allowPositionals: false, args, options }); }, {
    code: 'ERR_PARSE_ARGS_UNEXPECTED_POSITIONAL'
  });
});

// Test bad inputs

test('invalid argument passed for options', () => {
  const args = ['--so=wat'];
  const options = 'bad value';
  assert.throws(() => { parseArgs({ args, options }); }, {
    code: 'ERR_INVALID_ARG_TYPE'
  });
});

test('type property missing for option then throw', () => {
  const knownOptions = { foo: { } };
  assert.throws(() => { parseArgs({ options: knownOptions }); }, {
    code: 'ERR_INVALID_ARG_TYPE'
  });
});

test('boolean passed to "type" option', () => {
  const args = ['--so=wat'];
  const options = { foo: { type: true } };
  assert.throws(() => { parseArgs({ args, options }); }, {
    code: 'ERR_INVALID_ARG_TYPE'
  });
});

test('invalid union value passed to "type" option', () => {
  const args = ['--so=wat'];
  const options = { foo: { type: 'str' } };
  assert.throws(() => { parseArgs({ args, options }); }, {
    code: 'ERR_INVALID_ARG_TYPE'
  });
});

// Test strict mode

test('unknown long option --bar', () => {
  const args = ['--foo', '--bar'];
  const options = { foo: { type: 'boolean' } };
  assert.throws(() => { parseArgs({ args, options }); }, {
    code: 'ERR_PARSE_ARGS_UNKNOWN_OPTION'
  });
});

test('unknown short option -b', () => {
  const args = ['--foo', '-b'];
  const options = { foo: { type: 'boolean' } };
  assert.throws(() => { parseArgs({ args, options }); }, {
    code: 'ERR_PARSE_ARGS_UNKNOWN_OPTION'
  });
});

test('unknown option -r in short option group -bar', () => {
  const args = ['-bar'];
  const options = { b: { type: 'boolean' }, a: { type: 'boolean' } };
  assert.throws(() => { parseArgs({ args, options }); }, {
    code: 'ERR_PARSE_ARGS_UNKNOWN_OPTION'
  });
});

test('unknown option with explicit value', () => {
  const args = ['--foo', '--bar=baz'];
  const options = { foo: { type: 'boolean' } };
  assert.throws(() => { parseArgs({ args, options }); }, {
    code: 'ERR_PARSE_ARGS_UNKNOWN_OPTION'
  });
});

test('unexpected positional', () => {
  const args = ['foo'];
  const options = { foo: { type: 'boolean' } };
  assert.throws(() => { parseArgs({ args, options }); }, {
    code: 'ERR_PARSE_ARGS_UNEXPECTED_POSITIONAL'
  });
});

test('unexpected positional after --', () => {
  const args = ['--', 'foo'];
  const options = { foo: { type: 'boolean' } };
  assert.throws(() => { parseArgs({ args, options }); }, {
    code: 'ERR_PARSE_ARGS_UNEXPECTED_POSITIONAL'
  });
});

test('-- by itself is not a positional', () => {
  const args = ['--foo', '--'];
  const options = { foo: { type: 'boolean' } };
  const result = parseArgs({ args, options });
  const expected = { values: { __proto__: null, foo: true },
                     positionals: [] };
  assert.deepStrictEqual(result, expected);
});

test('string option used as boolean', () => {
  const args = ['--foo'];
  const options = { foo: { type: 'string' } };
  assert.throws(() => { parseArgs({ args, options }); }, {
    code: 'ERR_PARSE_ARGS_INVALID_OPTION_VALUE'
  });
});

test('boolean option used with value', () => {
  const args = ['--foo=bar'];
  const options = { foo: { type: 'boolean' } };
  assert.throws(() => { parseArgs({ args, options }); }, {
    code: 'ERR_PARSE_ARGS_INVALID_OPTION_VALUE'
  });
});

test('invalid short option length', () => {
  const args = [];
  const options = { foo: { short: 'fo', type: 'boolean' } };
  assert.throws(() => { parseArgs({ args, options }); }, {
    code: 'ERR_INVALID_ARG_VALUE'
  });
});

test('null prototype: when no options then values.toString is undefined', () => {
  const result = parseArgs({ args: [] });
  assert.strictEqual(result.values.toString, undefined);
});

test('null prototype: when --toString then values.toString is true', () => {
  const args = ['--toString'];
  const options = { toString: { type: 'boolean' } };
  const expectedResult = { values: { __proto__: null, toString: true }, positionals: [] };

  const result = parseArgs({ args, options });
  assert.deepStrictEqual(result, expectedResult);
});

const candidateGreedyOptions = [
  '',
  '-',
  '--',
  'abc',
  '123',
  '-s',
  '--foo',
];

candidateGreedyOptions.forEach((value) => {
  test(`greedy: when short option with value '${value}' then eaten`, () => {
    const args = ['-w', value];
    const options = { with: { type: 'string', short: 'w' } };
    const expectedResult = { values: { __proto__: null, with: value }, positionals: [] };

    const result = parseArgs({ args, options, strict: false });
    assert.deepStrictEqual(result, expectedResult);
  });

  test(`greedy: when long option with value '${value}' then eaten`, () => {
    const args = ['--with', value];
    const options = { with: { type: 'string', short: 'w' } };
    const expectedResult = { values: { __proto__: null, with: value }, positionals: [] };

    const result = parseArgs({ args, options, strict: false });
    assert.deepStrictEqual(result, expectedResult);
  });
});

test('strict: when candidate option value is plain text then does not throw', () => {
  const args = ['--with', 'abc'];
  const options = { with: { type: 'string' } };
  const expectedResult = { values: { __proto__: null, with: 'abc' }, positionals: [] };

  const result = parseArgs({ args, options, strict: true });
  assert.deepStrictEqual(result, expectedResult);
});

test("strict: when candidate option value is '-' then does not throw", () => {
  const args = ['--with', '-'];
  const options = { with: { type: 'string' } };
  const expectedResult = { values: { __proto__: null, with: '-' }, positionals: [] };

  const result = parseArgs({ args, options, strict: true });
  assert.deepStrictEqual(result, expectedResult);
});

test("strict: when candidate option value is '--' then throws", () => {
  const args = ['--with', '--'];
  const options = { with: { type: 'string' } };

  assert.throws(() => {
    parseArgs({ args, options });
  }, {
    code: 'ERR_PARSE_ARGS_INVALID_OPTION_VALUE'
  });
});

test('strict: when candidate option value is short option then throws', () => {
  const args = ['--with', '-a'];
  const options = { with: { type: 'string' } };

  assert.throws(() => {
    parseArgs({ args, options });
  }, {
    code: 'ERR_PARSE_ARGS_INVALID_OPTION_VALUE'
  });
});

test('strict: when candidate option value is short option digit then throws', () => {
  const args = ['--with', '-1'];
  const options = { with: { type: 'string' } };

  assert.throws(() => {
    parseArgs({ args, options });
  }, {
    code: 'ERR_PARSE_ARGS_INVALID_OPTION_VALUE'
  });
});

test('strict: when candidate option value is long option then throws', () => {
  const args = ['--with', '--foo'];
  const options = { with: { type: 'string' } };

  assert.throws(() => {
    parseArgs({ args, options });
  }, {
    code: 'ERR_PARSE_ARGS_INVALID_OPTION_VALUE'
  });
});

test('strict: when short option and suspect value then throws with short option in error message', () => {
  const args = ['-w', '--foo'];
  const options = { with: { type: 'string', short: 'w' } };

  assert.throws(() => {
    parseArgs({ args, options });
  }, /for '-w'/
  );
});

test('strict: when long option and suspect value then throws with long option in error message', () => {
  const args = ['--with', '--foo'];
  const options = { with: { type: 'string' } };

  assert.throws(() => {
    parseArgs({ args, options });
  }, /for '--with'/
  );
});

test('strict: when short option and suspect value then throws with whole expected message', () => {
  const args = ['-w', '--foo'];
  const options = { with: { type: 'string', short: 'w' } };

  try {
    parseArgs({ args, options });
  } catch (err) {
    console.info(err.message);
  }

  assert.throws(() => {
    parseArgs({ args, options });
  }, /To specify an option argument starting with a dash use '--with=-XYZ' or '-w-XYZ'/
  );
});

test('strict: when long option and suspect value then throws with whole expected message', () => {
  const args = ['--with', '--foo'];
  const options = { with: { type: 'string', short: 'w' } };

  assert.throws(() => {
    parseArgs({ args, options });
  }, /To specify an option argument starting with a dash use '--with=-XYZ'/
  );
});
