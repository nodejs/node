// Flags: --allow-async-hooks
'use strict';

require('../common');
const assert = require('assert');
const { createHook } = require('async_hooks');

// When --allow-async-hooks is passed, createHook should work
{
  // doesNotThrow
  const hook = createHook({
    init() {},
    before() {},
    after() {},
    destroy() {},
  });
  hook.enable();
  hook.disable();
  assert.ok(true, 'createHook works with --allow-async-hooks');
}
