// Copyright Joyent, Inc. and other Node contributors.
//
// Permission is hereby granted, free of charge, to any person obtaining a
// copy of this software and associated documentation files (the
// "Software"), to deal in the Software without restriction, including
// without limitation the rights to use, copy, modify, merge, publish,
// distribute, sublicense, and/or sell copies of the Software, and to permit
// persons to whom the Software is furnished to do so, subject to the
// following conditions:
//
// The above copyright notice and this permission notice shall be included
// in all copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
// OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
// MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN
// NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM,
// DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR
// OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE
// USE OR OTHER DEALINGS IN THE SOFTWARE.




var common = require('../common');
var assert = require('assert');

assert.ok(process.stdout.writable);
assert.ok(process.stderr.writable);
// Support legacy API
assert.equal('number', typeof process.stdout.fd);
assert.equal('number', typeof process.stderr.fd);

// an Object with a custom .inspect() function
var custom_inspect = { foo: 'bar', inspect: function () { return 'inspect'; } };

var stdout_write = global.process.stdout.write;
var strings = [];
global.process.stdout.write = function(string) {
  strings.push(string);
};
console._stderr = process.stdout;

// test console.log()
console.log('foo');
console.log('foo', 'bar');
console.log('%s %s', 'foo', 'bar', 'hop');
console.log({slashes: '\\\\'});
console.log(custom_inspect);

// test console.dir()
console.dir(custom_inspect);
console.dir(custom_inspect, { showHidden: false });
console.dir({ foo : { bar : { baz : true } } }, { depth: 0 });
console.dir({ foo : { bar : { baz : true } } }, { depth: 1 });

// test console.trace()
console.trace('This is a %j %d', { formatted: 'trace' }, 10, 'foo');


global.process.stdout.write = stdout_write;

assert.equal('foo\n', strings.shift());
assert.equal('foo bar\n', strings.shift());
assert.equal('foo bar hop\n', strings.shift());
assert.equal("{ slashes: '\\\\\\\\' }\n", strings.shift());
assert.equal('inspect\n', strings.shift());
assert.equal("{ foo: 'bar', inspect: [Function] }\n", strings.shift());
assert.equal("{ foo: 'bar', inspect: [Function] }\n", strings.shift());
assert.notEqual(-1, strings.shift().indexOf('foo: [Object]'));
assert.equal(-1, strings.shift().indexOf('baz'));
assert.equal('Trace: This is a {"formatted":"trace"} 10 foo',
             strings.shift().split('\n').shift());

assert.throws(function () {
  console.timeEnd('no such label');
});

assert.doesNotThrow(function () {
  console.time('label');
  console.timeEnd('label');
});
