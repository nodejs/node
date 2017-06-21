'use strict';

require('../common');
const assert = require('assert');

assert.throws(() => new Buffer(42, 'utf8'), /first argument must be a string/);
