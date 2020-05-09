'use strict';

const common = require('../common');
const assert = require('assert');
const fs = require('fs');
const { aliveResources } = require('async_hooks');

for (let i = 0; i < 12; i++)
  fs.open(__filename, 'r', common.mustCall());

assert.strictEqual(process._getActiveRequests().length, 12);
assert.strictEqual(Object.values(aliveResources()).length, 12);
