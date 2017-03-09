/* eslint-disable strict */
const common = require('../common');
const path = require('path');
const assert = require('assert');

common.globalCheck = false;

baseFoo = 'foo'; // eslint-disable-line no-undef
global.baseBar = 'bar';

assert.equal('foo', global.baseFoo, 'x -> global.x in base level not working');

assert.equal('bar',
             baseBar, // eslint-disable-line no-undef
             'global.x -> x in base level not working');

const mod = require(path.join(common.fixturesDir, 'global', 'plain'));
const fooBar = mod.fooBar;

assert.equal('foo', fooBar.foo, 'x -> global.x in sub level not working');

assert.equal('bar', fooBar.bar, 'global.x -> x in sub level not working');
