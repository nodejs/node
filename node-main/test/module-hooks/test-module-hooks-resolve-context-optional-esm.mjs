// Test that the context parameter can be omitted in the nextResolve invocation.

import * as common from '../common/index.mjs';
import { registerHooks } from 'node:module';

const hook = registerHooks({
  resolve: common.mustCall(function(specifier, context, nextResolve) {
    return nextResolve(specifier);
  }, 1),
});

await import('../fixtures/es-modules/message.mjs');

hook.deregister();
