'use strict';
require('../common');
var assert = require('assert');
var sys = require('sys'); // eslint-disable-line no-restricted-modules
var util = require('util');

assert.strictEqual(sys, util);
