// Test that the context parameter can be omitted in the nextLoad invocation.

import * as common from '../common/index.mjs';
import { registerHooks } from 'node:module';

const hook = registerHooks({
  load: common.mustCall(function(url, context, nextLoad) {
    return nextLoad(url);
  }, 1),
});

await import('../fixtures/es-modules/message.mjs');

hook.deregister();
