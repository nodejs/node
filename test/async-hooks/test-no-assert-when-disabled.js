'use strict';
const common = require('../common');

if (!common.isMainThread)
  common.skip('Workers don\'t inherit per-env state like the check flag');

if (!require('module').builtinModules.includes('internal/async_hooks')) {
  common.relaunchWithFlags(
    ['--expose-internals', '--no-force-async-hooks-checks']
  );
}

const async_hooks = require('internal/async_hooks');

// Negative asyncIds and invalid type name
async_hooks.emitInit(-1, null, -1, {});
async_hooks.emitBefore(-1, -1);
async_hooks.emitAfter(-1);
async_hooks.emitDestroy(-1);
