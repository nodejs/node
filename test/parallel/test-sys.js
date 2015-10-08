'use strict';
require('../common');
var assert = require('assert');
var sys = require('sys');

assert.equal('0', sys.inspect(0));
assert.equal('1', sys.inspect(1));
assert.equal('false', sys.inspect(false));
assert.equal("''", sys.inspect(''));
assert.equal("'hello'", sys.inspect('hello'));
assert.equal('[Function]', sys.inspect(function() {}));
assert.equal('undefined', sys.inspect(undefined));
assert.equal('null', sys.inspect(null));
assert.equal('/foo(bar\\n)?/gi', sys.inspect(/foo(bar\n)?/gi));
assert.equal(new Date('2010-02-14T12:48:40+01:00').toString(),
             sys.inspect(new Date('Sun, 14 Feb 2010 11:48:40 GMT')));

assert.equal("'\\n\\u0001'", sys.inspect('\n\u0001'));

assert.equal('[]', sys.inspect([]));
assert.equal('Array {}', sys.inspect(Object.create([])));
assert.equal('[ 1, 2 ]', sys.inspect([1, 2]));
assert.equal('[ 1, [ 2, 3 ] ]', sys.inspect([1, [2, 3]]));

assert.equal('{}', sys.inspect({}));
assert.equal('{ a: 1 }', sys.inspect({a: 1}));
assert.equal('{ a: [Function] }', sys.inspect({a: function() {}}));
assert.equal('{ a: 1, b: 2 }', sys.inspect({a: 1, b: 2}));
assert.equal('{ a: {} }', sys.inspect({'a': {}}));
assert.equal('{ a: { b: 2 } }', sys.inspect({'a': {'b': 2}}));
assert.equal('{ a: { b: { c: [Object] } } }',
             sys.inspect({'a': {'b': { 'c': { 'd': 2 }}}}));
assert.equal('{ a: { b: { c: { d: 2 } } } }',
             sys.inspect({'a': {'b': { 'c': { 'd': 2 }}}}, false, null));
assert.equal('[ 1, 2, 3, [length]: 3 ]', sys.inspect([1, 2, 3], true));
assert.equal('{ a: [Object] }',
             sys.inspect({'a': {'b': { 'c': 2}}}, false, 0));
assert.equal('{ a: { b: [Object] } }',
             sys.inspect({'a': {'b': { 'c': 2}}}, false, 1));
assert.equal('{ visible: 1 }',
    sys.inspect(Object.create({},
    {visible: {value: 1, enumerable: true}, hidden: {value: 2}}))
);

// Due to the hash seed randomization it's not deterministic the order that
// the following ways this hash is displayed.
// See http://codereview.chromium.org/9124004/

var out = sys.inspect(Object.create({},
    {visible: {value: 1, enumerable: true}, hidden: {value: 2}}), true);
if (out !== '{ [hidden]: 2, visible: 1 }' &&
    out !== '{ visible: 1, [hidden]: 2 }') {
  assert.ok(false);
}


// Objects without prototype
var out = sys.inspect(Object.create(null,
    { name: {value: 'Tim', enumerable: true},
      hidden: {value: 'secret'}}), true);
if (out !== "{ [hidden]: 'secret', name: 'Tim' }" &&
    out !== "{ name: 'Tim', [hidden]: 'secret' }") {
  assert(false);
}


assert.equal('{ name: \'Tim\' }',
    sys.inspect(Object.create(null,
                                 {name: {value: 'Tim', enumerable: true},
                                   hidden: {value: 'secret'}}))
);


// Dynamic properties
assert.equal('{ readonly: [Getter] }',
             sys.inspect({get readonly() {}}));

assert.equal('{ readwrite: [Getter/Setter] }',
             sys.inspect({get readwrite() {}, set readwrite(val) {}}));

assert.equal('{ writeonly: [Setter] }',
             sys.inspect({set writeonly(val) {}}));

var value = {};
value['a'] = value;
assert.equal('{ a: [Circular] }', sys.inspect(value));

// Array with dynamic properties
value = [1, 2, 3];
value.__defineGetter__('growingLength', function() {
  this.push(true); return this.length;
});
assert.equal('[ 1, 2, 3, growingLength: [Getter] ]', sys.inspect(value));

// Function with properties
value = function() {};
value.aprop = 42;
assert.equal('{ [Function] aprop: 42 }', sys.inspect(value));

// Regular expressions with properties
value = /123/ig;
value.aprop = 42;
assert.equal('{ /123/gi aprop: 42 }', sys.inspect(value));

// Dates with properties
value = new Date('Sun, 14 Feb 2010 11:48:40 GMT');
value.aprop = 42;
assert.equal('{ Sun, 14 Feb 2010 11:48:40 GMT aprop: 42 }',
             sys.inspect(value)
);
