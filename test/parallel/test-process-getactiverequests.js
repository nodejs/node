'use strict';

const { expectWarning } = require('../common');

expectWarning(
  'DeprecationWarning',
  'process._getActiveRequests() is deprecated. Please use the `async_hooks` ' +
  'module instead.',
  'DEP0157');

process._getActiveRequests();
