'use strict';
require('../../common');
var assert = require('assert');
var binding = require('./build/Release/binding');
assert.equal('world', binding.hello());
console.log('binding.hello() =', binding.hello());
