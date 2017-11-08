'use strict';
require('../common');
// Make sure the domain stack doesn't get clobbered by un-matched .exit()

const assert = require('assert');
const domain = require('domain');
const util = require('util');

const a = domain.create();
const b = domain.create();

a.enter(); // push
b.enter(); // push
assert.deepStrictEqual(domain._stack, [a, b], 'Unexpected stack shape ' +
                       `(domain._stack = ${util.inspect(domain._stack)})`);

domain.create().exit(); // no-op
assert.deepStrictEqual(domain._stack, [a, b], 'Unexpected stack shape ' +
                       `(domain._stack = ${util.inspect(domain._stack)})`);
