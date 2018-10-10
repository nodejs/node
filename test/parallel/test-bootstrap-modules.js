// Flags: --expose-internals
'use strict';

// This list must be computed before we require any modules to
// to eliminate the noise.
const list = process.moduleLoadList.slice();

const common = require('../common');
const assert = require('assert');

const isMainThread = common.isMainThread;
const kCoverageModuleCount = process.env.NODE_V8_COVERAGE ? 1 : 0;
const kMaxModuleCount = (isMainThread ? 64 : 86) + kCoverageModuleCount;

assert(list.length <= kMaxModuleCount,
       `Total length: ${list.length}\n` + list.join('\n')
);
