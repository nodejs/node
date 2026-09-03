'use strict';
const common = require('../common');

const domainWarning =
  'The `domain` module is deprecated and should not be used.';
common.expectWarning('DeprecationWarning', domainWarning, 'DEP0032');

const domain = require('domain');
const assert = require('assert');

assert.strictEqual(typeof domain.create, 'function');
assert.strictEqual(typeof domain.Domain, 'function');
