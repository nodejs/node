'use strict';

require('../common');
const { getCallSite } = require('node:util');
const { expectWarning } = require('../common');

const warning = 'The `util.getCallSite` API is deprecated. Please use `util.getCallSites()` instead.';
expectWarning('ExperimentalWarning', warning);
getCallSite();
