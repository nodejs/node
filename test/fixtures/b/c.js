const d = require('./d');

const assert = require('assert');

const package = require('./package');

assert.strictEqual('world', package.hello);

console.error('load fixtures/b/c.js');

let string = 'C';

// constructor
exports.SomeClass = function() {
};

exports.C = () => string;

exports.D = () => d.D();

process.on('exit', () => {
  string = 'C done';
  console.log('b/c.js exit');
});
