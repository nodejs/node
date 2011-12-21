var assert = require('assert');
var binding = require('./out/Release/binding');
assert.equal('world', binding.hello());
console.log('binding.hello() =', binding.hello());
