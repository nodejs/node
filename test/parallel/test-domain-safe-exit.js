'use strict';
// Make sure the domain stack doesn't get clobbered by un-matched .exit()

require('../common');
const assert = require('assert');
const domain = require('domain');

const a = domain.create();
const b = domain.create();

a.enter(); // push
b.enter(); // push
assert.deepStrictEqual(domain._stack, [a, b], 'b not pushed');

domain.create().exit(); // no-op
assert.deepStrictEqual(domain._stack, [a, b], 'stack mangled!');
