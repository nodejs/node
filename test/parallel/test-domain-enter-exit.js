'use strict';
// Make sure the domain stack is a stack

require('../common');
var assert = require('assert');
var domain = require('domain');

function names(array) {
  return array.map(function(d) {
    return d.name;
  }).join(', ');
}

var a = domain.create();
a.name = 'a';
var b = domain.create();
b.name = 'b';
var c = domain.create();
c.name = 'c';

a.enter(); // push
assert.deepStrictEqual(domain._stack, [a],
                       'a not pushed: ' + names(domain._stack));

b.enter(); // push
assert.deepStrictEqual(domain._stack, [a, b],
                       'b not pushed: ' + names(domain._stack));

c.enter(); // push
assert.deepStrictEqual(domain._stack, [a, b, c],
                       'c not pushed: ' + names(domain._stack));

b.exit(); // pop
assert.deepStrictEqual(domain._stack, [a],
                       'b and c not popped: ' + names(domain._stack));

b.enter(); // push
assert.deepStrictEqual(domain._stack, [a, b],
                       'b not pushed: ' + names(domain._stack));
