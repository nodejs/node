'use strict';
const common = require('../common');
const assert = require('assert');
const vm = require('vm');

if (typeof Intl === 'undefined')
  common.skip('missing Intl');

assert(!('v8BreakIterator' in Intl));
assert(!vm.runInNewContext('"v8BreakIterator" in Intl'));
