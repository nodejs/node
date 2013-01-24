var assert = require('assert');
var binding = require('./build/Release/binding');
assert.equal('world', binding());
console.log('binding.hello() =', binding());
