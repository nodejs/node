'use strict';
require('../common');

// Test some edge cases for AssertionError that are not tested elsewhere.
const assert = require('assert');

{
  const err = new assert.AssertionError({ operator: 'equal',
                                          actual: 'come on',
                                          expected: 'fhqwhgads',
  });

  assert.strictEqual(err.message,
                     "Expected values to be loosely equal:\n\n'come on'\n\n" +
                     "should loosely equal\n\n'fhqwhgads'");
}

{
  const err = new assert.AssertionError({ operator: 'notEqual',
                                          actual: 'come on',
                                          expected: 'fhqwhgads',
  });

  assert.strictEqual(err.message,
                     'Expected values not to be loosely equal:\n\n' +
                     "'come on'\n\nshould not loosely equal\n\n'fhqwhgads'");
}
