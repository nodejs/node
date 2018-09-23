'use strict';
var common = require('../common'),
    assert = require('assert'),
    repl = require('repl');

// https://github.com/joyent/node/issues/3226

require.cache.something = 1;
assert.equal(require.cache.something, 1);

repl.start({ useGlobal: false }).close();

assert.equal(require.cache.something, 1);
