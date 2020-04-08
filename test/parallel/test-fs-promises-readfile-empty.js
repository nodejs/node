'use strict';
require('../common');

const assert = require('assert');
const { promises: { readFile } } = require('fs');
const fixtures = require('../common/fixtures');

const fn = fixtures.path('empty.txt');

readFile(fn)
  .then(assert.ok);

readFile(fn, 'utf8')
  .then(assert.strictEqual.bind(this, ''));

readFile(fn, { encoding: 'utf8' })
  .then(assert.strictEqual.bind(this, ''));
