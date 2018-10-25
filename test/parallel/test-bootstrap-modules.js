/* eslint-disable node-core/required-modules */

'use strict';

// Ordinarily test files must require('common') but that action causes
// the global console to be compiled, defeating the purpose of this test.
// This makes sure no additional files are added without carefully considering
// lazy loading. Please adjust the value if necessary.
const kMaximumModulesExpected = 78;

const list = process.moduleLoadList.slice();

const assert = require('assert');

const numberModulesExpected = (() => {
  try {
    // if run with `--experimental-worker` there's one more module avalible;
    require('worker_threads');
    return kMaximumModulesExpected + 1;
  } catch {
    return kMaximumModulesExpected;
  }
})();

assert(list.length <= numberModulesExpected, list);
