// Flags: --expose-gc
'use strict';

require('../common');
const asyncHooks = require('async_hooks');
const vm = require('vm');

// This is a regression test for https://github.com/nodejs/node/issues/39019
//
// It should not segfault.

const hook = asyncHooks.createHook({ init() {} }).enable();
vm.createContext();
globalThis.gc();
hook.disable();
