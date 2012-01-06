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

assert.equal('0', common.inspect(0));
assert.equal('1', common.inspect(1));
assert.equal('false', common.inspect(false));
assert.equal("''", common.inspect(''));
assert.equal("'hello'", common.inspect('hello'));
assert.equal('[Function]', common.inspect(function() {}));
assert.equal('undefined', common.inspect(undefined));
assert.equal('null', common.inspect(null));
assert.equal('/foo(bar\\n)?/gi', common.inspect(/foo(bar\n)?/gi));
assert.equal('Sun, 14 Feb 2010 11:48:40 GMT',
             common.inspect(new Date('Sun, 14 Feb 2010 11:48:40 GMT')));

assert.equal("'\\n\\u0001'", common.inspect('\n\u0001'));

assert.equal('[]', common.inspect([]));
assert.equal('{}', common.inspect(Object.create([])));
assert.equal('[ 1, 2 ]', common.inspect([1, 2]));
assert.equal('[ 1, [ 2, 3 ] ]', common.inspect([1, [2, 3]]));

assert.equal('{}', common.inspect({}));
assert.equal('{ a: 1 }', common.inspect({a: 1}));
assert.equal('{ a: [Function] }', common.inspect({a: function() {}}));
assert.equal('{ a: 1, b: 2 }', common.inspect({a: 1, b: 2}));
assert.equal('{ a: {} }', common.inspect({'a': {}}));
assert.equal('{ a: { b: 2 } }', common.inspect({'a': {'b': 2}}));
assert.equal('{ a: { b: { c: [Object] } } }',
             common.inspect({'a': {'b': { 'c': { 'd': 2 }}}}));
assert.equal('{ a: { b: { c: { d: 2 } } } }',
             common.inspect({'a': {'b': { 'c': { 'd': 2 }}}}, false, null));
assert.equal('[ 1, 2, 3, [length]: 3 ]', common.inspect([1, 2, 3], true));
assert.equal('{ a: [Object] }',
             common.inspect({'a': {'b': { 'c': 2}}}, false, 0));
assert.equal('{ a: { b: [Object] } }',
             common.inspect({'a': {'b': { 'c': 2}}}, false, 1));
assert.equal('{ visible: 1 }',
    common.inspect(Object.create({},
    {visible: {value: 1, enumerable: true}, hidden: {value: 2}}))
);

// Due to the hash seed randomization it's not deterministic the order that
// the following ways this hash is displayed.
// See http://codereview.chromium.org/9124004/

var out = common.inspect(Object.create({},
    {visible: {value: 1, enumerable: true}, hidden: {value: 2}}), true);
if (out !== '{ [hidden]: 2, visible: 1 }' &&
    out !== '{ visible: 1, [hidden]: 2 }') {
  assert.ok(false);
}


// Objects without prototype
var out = common.inspect(Object.create(null,
    { name: {value: 'Tim', enumerable: true},
      hidden: {value: 'secret'}}), true);
if (out !== "{ [hidden]: 'secret', name: 'Tim' }" &&
    out !== "{ name: 'Tim', [hidden]: 'secret' }") {
  assert(false);
}


assert.equal('{ name: \'Tim\' }',
    common.inspect(Object.create(null,
                                 {name: {value: 'Tim', enumerable: true},
                                   hidden: {value: 'secret'}}))
);


// Dynamic properties
assert.equal('{ readonly: [Getter] }',
             common.inspect({get readonly() {}}));

assert.equal('{ readwrite: [Getter/Setter] }',
             common.inspect({get readwrite() {},set readwrite(val) {}}));

assert.equal('{ writeonly: [Setter] }',
             common.inspect({set writeonly(val) {}}));

var value = {};
value['a'] = value;
assert.equal('{ a: [Circular] }', common.inspect(value));

// Array with dynamic properties
value = [1, 2, 3];
value.__defineGetter__('growingLength', function() {
  this.push(true); return this.length;
});
assert.equal('[ 1, 2, 3, growingLength: [Getter] ]', common.inspect(value));

// Function with properties
value = function() {};
value.aprop = 42;
assert.equal('{ [Function] aprop: 42 }', common.inspect(value));

// Regular expressions with properties
value = /123/ig;
value.aprop = 42;
assert.equal('{ /123/gi aprop: 42 }', common.inspect(value));

// Dates with properties
value = new Date('Sun, 14 Feb 2010 11:48:40 GMT');
value.aprop = 42;
assert.equal('{ Sun, 14 Feb 2010 11:48:40 GMT aprop: 42 }',
             common.inspect(value)
);
