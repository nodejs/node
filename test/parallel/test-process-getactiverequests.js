'use strict';

require('../common');
const assert = require('assert');
const fs = require('fs');

for (let i = 0; i < 12; i++)
  fs.open(__filename, 'r', () => {});

assert.strictEqual(12, process._getActiveRequests().length);
