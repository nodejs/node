/* eslint-disable strict */
const common = require('../common');
const assert = require('assert');

common.globalCheck = false;

baseFoo = 'foo';
global.baseBar = 'bar';

assert.equal('foo', global.baseFoo, 'x -> global.x in base level not working');

assert.equal('bar', baseBar, 'global.x -> x in base level not working');

const fooBar = require('../fixtures/global/plain').fooBar;

assert.equal('foo', fooBar.foo, 'x -> global.x in sub level not working');

assert.equal('bar', fooBar.bar, 'global.x -> x in sub level not working');
