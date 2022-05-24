// Flags: --expose-internals
'use strict';

require('../common');
const assert = require('assert');
const { validateOneOf } = require('internal/validators');

{
  // validateOneOf number incorrect.
  const allowed = [2, 3];
  assert.throws(() => validateOneOf(1, 'name', allowed), {
    code: 'ERR_INVALID_ARG_VALUE',
  });
}

{
  // validateOneOf number correct.
  validateOneOf(2, 'name', [1, 2]);
}

{
  // validateOneOf string incorrect.
  const allowed = ['b', 'c'];
  assert.throws(() => validateOneOf('a', 'name', allowed), {
    code: 'ERR_INVALID_ARG_VALUE',
  });
}

{
  // validateOneOf string correct.
  validateOneOf('two', 'name', ['one', 'two']);
}

{
  // validateOneOf Symbol incorrect.
  const allowed = [Symbol.for('b'), Symbol.for('c')];
  assert.throws(() => validateOneOf(Symbol.for('a'), 'name', allowed), {
    code: 'ERR_INVALID_ARG_VALUE',
  });
}

{
  // validateOneOf Symbol correct.
  const allowed = [Symbol.for('b'), Symbol.for('c')];
  validateOneOf(Symbol.for('b'), 'name', allowed);
}
