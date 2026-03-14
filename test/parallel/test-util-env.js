'use strict';

const common = require('../common');
const assert = require('assert');
const util = require('util');

// Test util.stringifyEnv
{
  const simple = { A: '1', B: '2' };
  assert.strictEqual(util.stringifyEnv(simple), 'A=1\nB=2');
}

{
  const quotes = { A: '1 "2" 3' };
  assert.strictEqual(util.stringifyEnv(quotes), 'A="1 \\"2\\" 3"');
}

{
  const newlines = { A: '1\n2' };
  assert.strictEqual(util.stringifyEnv(newlines), 'A="1\\n2"');
}

{
  const empty = {};
  assert.strictEqual(util.stringifyEnv(empty), '');
}

{
  const complex = {
    A: 'val_a',
    B: 'val_b',
    C: 'val with spaces',
    D: 'val_with_"quotes"',
    E: 'val_with_\n_newlines'
  };
  const expected = 'A=val_a\n' +
                   'B=val_b\n' +
                   'C="val with spaces"\n' +
                   'D="val_with_\\"quotes\\""\n' +
                   'E="val_with_\\n_newlines"';
  assert.strictEqual(util.stringifyEnv(complex), expected);
}

// Test validation
{
  assert.throws(() => util.stringifyEnv(null), {
    code: 'ERR_INVALID_ARG_TYPE',
  });
  assert.throws(() => util.stringifyEnv('string'), {
    code: 'ERR_INVALID_ARG_TYPE',
  });
}
