'use strict';
const common = require('../common');
const assert = require('assert');

assert.throws(function() {
  require('internal/freelist');
});

assert(require('../fixtures/internal-modules') === 42);
