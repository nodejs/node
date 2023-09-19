// Flags: --experimental-shadow-realm
'use strict';

require('../common');
const assert = require('assert');

let principalRealmPrepareStackTraceCalled = false;
Error.prepareStackTrace = (error, trace) => {
  principalRealmPrepareStackTraceCalled = true;
  return `${String(error)}\n    at ${trace.join('\n    at ')}`;
};

{
  // Validates inner Error.prepareStackTrace can not leak into the outer realm.
  const shadowRealm = new ShadowRealm();

  const stack = shadowRealm.evaluate(`
Error.prepareStackTrace = (error, trace) => {
  globalThis.leaked = 'inner';
  return 'from shadow realm';
};

try {
  throw new Error('boom');
} catch (e) {
  e.stack;
}
`);
  assert.ok(!principalRealmPrepareStackTraceCalled);
  assert.strictEqual(stack, 'from shadow realm');
  assert.strictEqual('leaked' in globalThis, false);
}

{
  // Validates stacks can be generated in the ShadowRealm.
  const shadowRealm = new ShadowRealm();

  const stack = shadowRealm.evaluate(`
function myFunc() {
  throw new Error('boom');
}

try {
  myFunc();
} catch (e) {
  e.stack;
}
`);
  assert.ok(!principalRealmPrepareStackTraceCalled);
  const lines = stack.split('\n');
  assert.strictEqual(lines[0], 'Error: boom');
  assert.match(lines[1], /^ {4}at myFunc \(.*\)/);
}
