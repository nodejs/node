'use strict';
// Flags: --no-force-async-hooks-checks --expose-internals
const common = require('../common');

const async_hooks = require('internal/async_hooks');
const internal = require('internal/process/next_tick');

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
