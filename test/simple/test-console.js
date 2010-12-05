var common = require('../common');
var assert = require('assert');

var stdout_write = global.process.stdout.write;
var strings = [];
global.process.stdout.write = function(string) {
  strings.push(string);
};

console.log('foo');
console.log('foo', 'bar');
console.log('%s %s', 'foo', 'bar', 'hop');

global.process.stdout.write = stdout_write;
assert.equal('foo\n', strings.shift());
assert.equal('foo bar\n', strings.shift());
assert.equal('foo bar hop\n', strings.shift());
