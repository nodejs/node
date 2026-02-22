'use strict';

// Flags: -C additional -C additional2

require('../common');
const assert = require('node:assert');
const { customConditions } = require('module');

assert.deepStrictEqual(
  customConditions,
  new Set(['additional', 'additional2'])
);
