'use strict';
const common = require('../common');
const assert = require('assert');

common.expectWarning(
  'DeprecationWarning',
  'Module.prototype.parent is deprecated and may be removed in a later ' +
  'version of Node.js.',
  'DEP0143'
);

assert.strictEqual(module.parent, null);
