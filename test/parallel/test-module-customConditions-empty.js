'use strict';

// Flags:

require('../common');
const assert = require('node:assert');
const { customConditions } = require('module');

assert.deepStrictEqual(customConditions, new Set());
