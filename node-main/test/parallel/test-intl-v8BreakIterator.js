'use strict';
const common = require('../common');
const assert = require('assert');
const vm = require('vm');

if (!common.hasIntl)
  common.skip('missing Intl');

assert(!('v8BreakIterator' in Intl));
assert(!vm.runInNewContext('"v8BreakIterator" in Intl'));
