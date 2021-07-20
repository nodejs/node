'use strict';

require('../common');
const fixtures = require('../common/fixtures');

// Accessing module.parent from a child module should not raise a warning.
require(fixtures.path('cjs-module-parent.js'));
