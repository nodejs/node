// Flags: --allow-async-hooks
'use strict';

require('../common');
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
}
