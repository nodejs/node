'use strict';
// There might be circular dependency issues between these modules
require('../common');
const assert = require('assert');
const AsyncHooks = require('async_hooks');
const AsyncLocalStorage = require('async_local_storage');

assert.strictEqual(AsyncHooks.AsyncLocalStorage, AsyncLocalStorage);
