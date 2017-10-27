'use strict';

require('../common');
const assert = require('assert');
const natives = process.binding('natives');
const {
  isCoreModule,
  getCoreModules
} = require('module');

const keys = Object.keys(natives);

keys.filter((name) => !name.startsWith('internal/'))
  .forEach((i) => assert(isCoreModule(i)));

keys.filter((name) => name.startsWith('internal/'))
  .forEach((i) => assert(!isCoreModule(i)));

['foo', 'bar', 'baz']
  .forEach((i) => assert(!isCoreModule(i)));

const list = getCoreModules();

assert(Array.isArray(list));
assert.deepStrictEqual(list,
                       keys.filter((name) => !name.startsWith('internal/')));
