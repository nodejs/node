'use strict';
require('../common');
const assert = require('assert');
const repl = require('repl');

// https://github.com/joyent/node/issues/3226

require.cache.something = 1;
assert.equal(require.cache.something, 1);

repl.start({ useGlobal: false }).close();

assert.equal(require.cache.something, 1);
