'use strict';
require('../common');

const { describe, it, beforeEach } = require('node:test');
const { strictEqual } = require('node:assert');

const { diff } = require('util');

describe('diff', () => {
  beforeEach(() => {
    process.env.NODE_DISABLE_COLORS = 'true';
  });

  it('returns null because actual and expected are the same', () => {
    const actual = 'foo';
    const expected = 'foo';

    const result = diff(actual, expected);
    strictEqual(result, null);
  });

  it('returns the simple short diff for strings', () => {
    const actual = 'foobar';
    const expected = 'foosar';
    const result = diff(actual, expected);

    strictEqual(result, "\n'foobar' !== 'foosar'\n");
  });

  it('returns the long not colored diff for strings', () => {
    const actual = 'foobarfoo';
    const expected = 'barfoobar';
    const result = diff(actual, expected);

    strictEqual(result, "+ actual - expected\n\n+ 'foobarfoo'\n- 'barfoobar'\n");
  });

  it('returns the long diff for strings, with indicator', () => {
    const actual = '123456789ABCDEFGHI';
    const expected = '12!!5!7!9!BC!!!GHI';
    const result = diff(actual, expected);

    strictEqual(result, "+ actual - expected\n\n+ '123456789ABCDEFGHI'\n- '12!!5!7!9!BC!!!GHI'\n     ^\n");
  });

  it('returns the diff for objects', () => {
    const actual = { foo: 'bar' };
    const expected = { foo: 'baz' };
    const result = diff(actual, expected);

    strictEqual(result, "+ actual - expected\n\n  {\n+   foo: 'bar'\n-   foo: 'baz'\n  }\n");
  });

  it('returns the diff for arrays', () => {
    const actual = [1, 2, 3];
    const expected = [1, 3, 4];
    const result = diff(actual, expected);

    strictEqual(result, '+ actual - expected\n\n  [\n    1,\n+   2,\n    3,\n-   4\n  ]\n');
  });

  it('returns the long diff for strings, colored', () => {
    process.env.FORCE_COLOR = '3';
    const actual = '123456789ABCDEFGHI';
    const expected = '12!!5!7!9!BC!!!GHI';
    const result = diff(actual, expected);

    strictEqual(
      result,
      '\x1B[32mactual\x1B[39m \x1B[31mexpected\x1B[39m\n' +
        '\n' +
        "\x1B[39m'\x1B[39m\x1B[39m1\x1B[39m\x1B[39m2\x1B[39m\x1B[32m3\x1B[39m\x1B[32m4\x1B[39m" +
        '\x1B[31m!\x1B[39m\x1B[31m!\x1B[39m\x1B[39m5\x1B[39m\x1B[32m6\x1B[39m\x1B[31m!\x1B[39m' +
        '\x1B[39m7\x1B[39m\x1B[32m8\x1B[39m\x1B[31m!\x1B[39m\x1B[39m9\x1B[39m\x1B[32mA\x1B[39m' +
        '\x1B[31m!\x1B[39m\x1B[39mB\x1B[39m\x1B[39mC\x1B[39m\x1B[32mD\x1B[39m\x1B[32mE\x1B[39m' +
        '\x1B[32mF\x1B[39m\x1B[31m!\x1B[39m\x1B[31m!\x1B[39m\x1B[31m!\x1B[39m\x1B[39mG\x1B[39m' +
        "\x1B[39mH\x1B[39m\x1B[39mI\x1B[39m\x1B[39m'\x1B[39m\n"
    );
  });
});
