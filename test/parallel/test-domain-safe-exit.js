'use strict';
// Make sure the domain stack doesn't get clobbered by un-matched .exit()

require('../common');
var assert = require('assert');
var domain = require('domain');

var a = domain.create();
var b = domain.create();

a.enter(); // push
b.enter(); // push
assert.deepStrictEqual(domain._stack, [a, b], 'b not pushed');

domain.create().exit(); // no-op
assert.deepStrictEqual(domain._stack, [a, b], 'stack mangled!');
