// Flags: --expose-internals
'use strict';

// This list must be computed before we require any modules to
// to eliminate the noise.
const list = process.moduleLoadList.slice();

require('../common');
const assert = require('assert');

// We use the internals to detect if this test is run in a worker
// to eliminate the noise introduced by --experimental-worker
const { internalBinding } = require('internal/test/binding');
const { threadId } = internalBinding('worker');
const isMainThread = threadId === 0;
const kMaxModuleCount = isMainThread ? 56 : 78;

assert(list.length <= kMaxModuleCount,
       `Total length: ${list.length}\n` + list.join('\n')
);
