// Flags: --expose-internals
'use strict';

require('../common');
const assert = require('assert');
const { createHook } = require('async_hooks');
const { enabledHooksExist } = require('internal/async_hooks');

assert.strictEqual(enabledHooksExist(), false);

const ah = createHook({});
assert.strictEqual(enabledHooksExist(), false);

ah.enable();
assert.strictEqual(enabledHooksExist(), true);

ah.disable();
assert.strictEqual(enabledHooksExist(), false);
