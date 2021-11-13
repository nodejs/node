'use strict';

const { expectWarning } = require('../common');

expectWarning(
  'DeprecationWarning',
  'process._getActiveHandles() is deprecated. Please use the `async_hooks` ' +
  'module instead.',
  'DEP0157');

process._getActiveHandles();
