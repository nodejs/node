'use strict';
const assert = require('assert');
const binding = require('./build/Release/binding');
assert.equal('world', binding.hello());
console.log('binding.hello() =', binding.hello());
