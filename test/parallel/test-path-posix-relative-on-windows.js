'use strict';

require('../common');
const assert = require('assert');
const path = require('path');

// Refs: https://github.com/nodejs/node/issues/13683

const relativePath = path.posix.relative('a/b/c', '../../x');
assert.match(relativePath, /^(\.\.\/){3,5}x$/);
