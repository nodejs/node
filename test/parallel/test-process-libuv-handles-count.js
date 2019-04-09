'use strict';

const common = require('../common');
const assert = require('assert');
const fs = require('fs');
const path = require('path');

if (!common.isMainThread)
  common.skip('process.libuvHandlesCount is not available in Workers');

const count = process.libuvHandlesCount();

assert(typeof count.total === 'number');
assert(count.each.length === 18);

const sum = count.each.reduce((sum, c) => sum + c, 0);
assert(sum === count.total);
