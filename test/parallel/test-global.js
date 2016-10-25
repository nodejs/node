/* eslint-disable strict */
const common = require('../common');
const path = require('path');
const assert = require('assert');

common.globalCheck = false;

baseFoo = 'foo'; // eslint-disable-line no-undef
global.baseBar = 'bar';

assert.strictEqual('foo', global.baseFoo,
                   'x -> global.x in base level not working');

assert.strictEqual('bar',
                   baseBar, // eslint-disable-line no-undef
                   'global.x -> x in base level not working');

var module = require(path.join(common.fixturesDir, 'global', 'plain'));
const fooBar = module.fooBar;

assert.strictEqual('foo', fooBar.foo, 'x -> global.x in sub level not working');

assert.strictEqual('bar', fooBar.bar, 'global.x -> x in sub level not working');

assert.strictEqual(Object.prototype.toString.call(global), '[object global]');
