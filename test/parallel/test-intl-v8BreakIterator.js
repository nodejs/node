'use strict';
require('../common');
const assert = require('assert');

if (global.Intl === undefined || Intl.v8BreakIterator === undefined) {
  return console.log('1..0 # Skipped: no Intl');
}

try {
  new Intl.v8BreakIterator();
  // May succeed if data is available - OK
} catch (e) {
  // May throw this error if ICU data is not available - OK
  assert.throws(() => new Intl.v8BreakIterator(), /ICU data/);
}
