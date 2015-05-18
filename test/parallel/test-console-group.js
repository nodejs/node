var common = require('../common');
var assert = require('assert');

assert.ok(process.stdout.writable);
assert.ok(process.stderr.writable);
// Support legacy API
assert.equal('number', typeof process.stdout.fd);
assert.equal('number', typeof process.stderr.fd);

assert.doesNotThrow(function () {
  console.group('label');
  console.groupEnd();
});

assert.doesNotThrow(function () {
    console.groupCollapsed('label');
    console.groupEnd();
});

// test multiple calls to console.groupEnd()
assert.doesNotThrow(function () {
    console.groupEnd();
    console.groupEnd();
    console.groupEnd();
});

var stdout_write = global.process.stdout.write;
var strings = [];
global.process.stdout.write = function(string) {
  strings.push(string);
};
console._stderr = process.stdout;

// test console.group(), console.groupCollapsed() and console.groupEnd()
console.log('foo');
console.group('bar');
console.log('foo bar');
console.groupEnd();
console.groupCollapsed('group 1');
console.log('log');
console.warn('warn');
console.error('error');
console.dir({ foo: true });
console.group('group 2');
console.log('foo bar hop');
console.log('line 1\nline 2\nline 3');
console.groupEnd();
console.log('end 2');
console.groupEnd();
console.log('end 1');

global.process.stdout.write = stdout_write;

assert.equal('foo\n', strings.shift());
assert.equal('bar\n', strings.shift());
assert.equal('  foo bar\n', strings.shift());
assert.equal("group 1\n", strings.shift());
assert.equal('  log\n', strings.shift());
assert.equal('  warn\n', strings.shift());
assert.equal('  error\n', strings.shift());
assert.equal('  { foo: true }\n', strings.shift());
assert.equal('  group 2\n', strings.shift());
assert.equal('    foo bar hop\n', strings.shift());
assert.equal('    line 1\n    line 2\n    line 3\n', strings.shift());
assert.equal('  end 2\n', strings.shift());
assert.equal('end 1\n', strings.shift());
assert.equal(strings.length, 0);