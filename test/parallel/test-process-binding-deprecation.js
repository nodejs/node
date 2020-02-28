'use strict';
const common = require('../common');

const warn = 'process.binding() is deprecated. Please use public APIs instead.';
common.expectWarning('DeprecationWarning', warn, 'DEP0111');

process.binding('util');
