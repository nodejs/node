'use strict';
// There might be circular dependency issues between these modules
require('../common');
const assert = require('assert');
const AsyncLocalStorage = require('async_local_storage');
const AsyncHooks = require('async_hooks');

assert.strictEqual(AsyncHooks.AsyncLocalStorage, AsyncLocalStorage);
