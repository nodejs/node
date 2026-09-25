// Test that the context parameter will be merged in multiple load hooks.

import * as common from '../common/index.mjs';
import assert from 'node:assert';
import { registerHooks } from 'node:module';

const hook1 = registerHooks({
  load: common.mustCall(function(url, context, nextLoad) {
    assert.strictEqual(context.testProp, 'custom');  // It should be merged from hook 2 and 3.
    return nextLoad(url, context);
  }, 1),
});

const hook2 = registerHooks({
  load: common.mustCall(function(url, context, nextLoad) {
    assert.strictEqual(context.testProp, 'custom');  // It should be merged from hook 3.
    return nextLoad(url);  // Omit the context.
  }, 1),
});

const hook3 = registerHooks({
  load: common.mustCall(function(url, context, nextLoad) {
    return nextLoad(url, { testProp: 'custom' });  // Add a custom property
  }, 1),
});

await import('../fixtures/es-modules/message.mjs');

hook3.deregister();
hook2.deregister();
hook1.deregister();
