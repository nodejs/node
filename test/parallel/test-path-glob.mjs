import '../common/index.mjs';
import { describe, it } from 'node:test';
import * as assert from 'node:assert';
import * as path from 'node:path';


// https://github.com/torvalds/linux/blob/cdc9718d5e590d6905361800b938b93f2b66818e/lib/globtest.c
const patterns = [
  { expected: true, pattern: 'a', name: 'a' },
  { expected: false, pattern: 'a', name: 'b' },
  { expected: false, pattern: 'a', name: 'aa' },
  { expected: false, pattern: 'a', name: '' },
  { expected: true, pattern: '', name: '' },
  { expected: false, pattern: '', name: 'a' },
  /* Simple character class tests */
  { expected: true, pattern: '[a]', name: 'a' },
  { expected: false, pattern: '[a]', name: 'b' },
  { expected: false, pattern: '[!a]', name: 'a' },
  { expected: true, pattern: '[!a]', name: 'b' },
  { expected: true, pattern: '[ab]', name: 'a' },
  { expected: true, pattern: '[ab]', name: 'b' },
  { expected: false, pattern: '[ab]', name: 'c' },
  { expected: true, pattern: '[!ab]', name: 'c' },
  { expected: true, pattern: '[a-c]', name: 'b' },
  { expected: false, pattern: '[a-c]', name: 'd' },
  /* Corner cases in character class parsing */
  { expected: true, pattern: '[a-c-e-g]', name: '-' },
  { expected: false, pattern: '[a-c-e-g]', name: 'd' },
  { expected: true, pattern: '[a-c-e-g]', name: 'f' },
  { expected: true, pattern: '[]a-ceg-ik[]', name: 'a' },
  { expected: true, pattern: '[]a-ceg-ik[]', name: ']' },
  { expected: true, pattern: '[]a-ceg-ik[]', name: '[' },
  { expected: true, pattern: '[]a-ceg-ik[]', name: 'h' },
  { expected: false, pattern: '[]a-ceg-ik[]', name: 'f' },
  { expected: false, pattern: '[!]a-ceg-ik[]', name: 'h' },
  { expected: false, pattern: '[!]a-ceg-ik[]', name: ']' },
  { expected: true, pattern: '[!]a-ceg-ik[]', name: 'f' },
  /* Simple wild cards */
  { expected: true, pattern: '?', name: 'a' },
  { expected: false, pattern: '?', name: 'aa' },
  { expected: false, pattern: '??', name: 'a' },
  { expected: true, pattern: '?x?', name: 'axb' },
  { expected: false, pattern: '?x?', name: 'abx' },
  { expected: false, pattern: '?x?', name: 'xab' },
  /* Asterisk wild cards (backtracking) */
  { expected: false, pattern: '*??', name: 'a' },
  { expected: true, pattern: '*??', name: 'ab' },
  { expected: true, pattern: '*??', name: 'abc' },
  { expected: true, pattern: '*??', name: 'abcd' },
  { expected: false, pattern: '??*', name: 'a' },
  { expected: true, pattern: '??*', name: 'ab' },
  { expected: true, pattern: '??*', name: 'abc' },
  { expected: true, pattern: '??*', name: 'abcd' },
  { expected: false, pattern: '?*?', name: 'a' },
  { expected: true, pattern: '?*?', name: 'ab' },
  { expected: true, pattern: '?*?', name: 'abc' },
  { expected: true, pattern: '?*?', name: 'abcd' },
  { expected: true, pattern: '*b', name: 'b' },
  { expected: true, pattern: '*b', name: 'ab' },
  { expected: false, pattern: '*b', name: 'ba' },
  { expected: true, pattern: '*b', name: 'bb' },
  { expected: true, pattern: '*b', name: 'abb' },
  { expected: true, pattern: '*b', name: 'bab' },
  { expected: true, pattern: '*bc', name: 'abbc' },
  { expected: true, pattern: '*bc', name: 'bc' },
  { expected: true, pattern: '*bc', name: 'bbc' },
  { expected: true, pattern: '*bc', name: 'bcbc' },
  /* Multiple asterisks (complex backtracking) */
  { expected: true, pattern: '*ac*', name: 'abacadaeafag' },
  { expected: true, pattern: '*ac*ae*ag*', name: 'abacadaeafag' },
  { expected: true, pattern: '*a*b*[bc]*[ef]*g*', name: 'abacadaeafag' },
  { expected: false, pattern: '*a*b*[ef]*[cd]*g*', name: 'abacadaeafag' },
  { expected: true, pattern: '*abcd*', name: 'abcabcabcabcdefg' },
  { expected: true, pattern: '*ab*cd*', name: 'abcabcabcabcdefg' },
  { expected: true, pattern: '*abcd*abcdef*', name: 'abcabcdabcdeabcdefg' },
  { expected: false, pattern: '*abcd*', name: 'abcabcabcabcefg' },
  { expected: false, pattern: '*ab*cd*', name: 'abcabcabcabcefg' },
];

const invalid = [null, undefined, 1, Number.MAX_SAFE_INTEGER, true, false, Symbol(), {}, [], () => {}];

describe('path.glob', () => {
  for (const { expected, pattern, name } of patterns) {
    it(`pattern "${pattern}" should ${expected ? '' : 'not '}match "${name}"`, () => {
      assert.strictEqual(path.glob(pattern, name), expected);
    });
  }

  for (const x of invalid) {
    const name = typeof x === 'symbol' ? 'Symnol()' : x;
    it(`${name} should throw as a parameter`, () => {
      assert.throws(() => path.glob(x, ''), { code: 'ERR_INVALID_ARG_TYPE' });
      assert.throws(() => path.glob('', x), { code: 'ERR_INVALID_ARG_TYPE' });
    });
  }
});
