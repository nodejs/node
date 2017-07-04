'use strict';
const common = require('../common');

if (!common.hasIntl || Intl.v8BreakIterator === undefined)
  common.skip('missing Intl');

const assert = require('assert');
const warning = 'Intl.v8BreakIterator is deprecated and will be removed soon.';
common.expectWarning('DeprecationWarning', warning);

try {
  new Intl.v8BreakIterator();
  // May succeed if data is available - OK
} catch (e) {
  // May throw this error if ICU data is not available - OK
  assert.throws(() => new Intl.v8BreakIterator(), /ICU data/);
}
