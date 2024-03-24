'use strict';

require('../common');
const assert = require('assert');

// Unreachable should error for any value truthy or falsy
[ true, false, null, undefined, '', 'type' ].forEach((exampleValue) => {
  assert.throws(
    () => { assert.unreachable(exampleValue); },
    {
      code: 'ERR_ASSERTION',
      name: 'AssertionError',
      message: 'Expected this assertion to be unreachable',
      operator: 'unreachable',
      actual: exampleValue,
      expected: undefined,
      generatedMessage: true,
      stack: /to be unreachable/
    }
  );
});

// Message can be changed
assert.throws(
  () => { assert.unreachable(null, 'whoops'); },
  {
    code: 'ERR_ASSERTION',
    name: 'AssertionError',
    message: 'whoops',
    operator: 'unreachable',
    actual: null,
    expected: undefined,
    generatedMessage: false,
    stack: /whoops/
  }
);

// Or the whole error can be replaced
const err = new Error();
assert.throws(
  () => { assert.unreachable(null, err); },
  err,
);

// The `actual` argument is required
assert.throws(
  () => { assert.unreachable(); },
  {
    code: 'ERR_MISSING_ARGS',
  }
);
