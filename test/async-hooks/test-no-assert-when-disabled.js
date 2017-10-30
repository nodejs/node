'use strict';
// Flags: --expose-internals
const common = require('../common');

const async_hooks = require('async_hooks');
const internal = require('internal/process/next_tick');

// In tests async_hooks dynamic checks are enabled by default. To verify
// that no checks are enabled ordinarily disable the checks in this test.
common.revert_force_async_hooks_checks();

// When async_hooks is diabled (or never enabled), the checks
// should be disabled as well. This is important while async_hooks is
// experimental and there are still critial bugs to fix.

// Using undefined as the triggerAsyncId.
// Ref: https://github.com/nodejs/node/issues/14386
// Ref: https://github.com/nodejs/node/issues/14381
// Ref: https://github.com/nodejs/node/issues/14368
internal.nextTick(undefined, common.mustCall());

// Negative asyncIds and invalid type name
async_hooks.emitInit(-1, null, -1, {});
async_hooks.emitBefore(-1, -1);
async_hooks.emitAfter(-1);
async_hooks.emitDestroy(-1);
