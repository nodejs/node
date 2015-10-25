'use strict';
const common = require('../common');
const assert = require('assert');
const fork = require('child_process').fork;

const n = fork(common.fixturesDir + '/empty.js');

const rv = n.send({ hello: 'world' });
assert.strictEqual(rv, true);
