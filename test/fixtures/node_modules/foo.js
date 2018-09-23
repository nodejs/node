console.error(__filename);
console.error(module.paths.join('\n') + '\n');
var assert = require('assert');
assert.equal(require('baz'), require('./baz/index.js'));
