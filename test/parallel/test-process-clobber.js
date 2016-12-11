/* eslint-disable no-global-assign */
'use strict';

require('../common');
const assert = require('assert');

assert.throws(() => process = null, /Cannot assign to read only property/);
