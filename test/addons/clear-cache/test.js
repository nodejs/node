'use strict';
require('../../common');
const assert = require('assert');

function clearCache() {
  Object.keys(require.cache).forEach((key) => delete require.cache[key]);
}

const val1 = require('./build/release/binding.node').hello;
clearCache();
const val2 = require('./build/release/binding.node').hello;

assert(val1 == val2);
