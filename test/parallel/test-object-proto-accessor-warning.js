// Flags: --pending-deprecation
'use strict';

const common = require('../common');

common.expectWarning(
  'DeprecationWarning',
  '__proto__ is deprecated, use Object.getPrototypeOf instead', 'DEP0XXX');

const obj = {};
// eslint-disable-next-line
const _ = obj.__proto__;
// eslint-disable-next-line
obj.__proto__ = null;
