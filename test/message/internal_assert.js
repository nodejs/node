'use strict';

// Flags: --expose-internals
require('../common');

const assert = require('internal/assert');
assert(2 + 2 > 5);
