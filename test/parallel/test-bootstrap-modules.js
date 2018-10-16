/* eslint-disable node-core/required-modules */

'use strict';

// Ordinarily test files must require('common') but that action causes
// the global console to be compiled, defeating the purpose of this test.
// This makes sure no additional files are added without carefully considering
// lazy loading. Please adjust the value if necessary.

const list = process.moduleLoadList.slice();

const assert = require('assert');

assert(list.length <= 83,
       `Expected <= 83 elements in moduleLoadLists, got ${list.length}`);
