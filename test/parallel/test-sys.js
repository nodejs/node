'use strict';
require('../common');
const assert = require('assert');
const sys = require('sys'); // eslint-disable-line no-restricted-modules
const util = require('util');

assert.strictEqual(sys, util);
