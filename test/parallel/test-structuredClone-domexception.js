'use strict';

require('../common');
const assert = require('assert');

function assertDOMException(actual, expected) {
  assert.strictEqual(actual instanceof DOMException, true);
  assert.strictEqual(actual.message, expected.message);
  assert.strictEqual(actual.name, expected.name);
  assert.strictEqual(actual.code, expected.code);
  assert.strictEqual(actual.stack, expected.stack);
}

{
  // Clone basic DOMException
  const e = new DOMException('test');
  const clone = structuredClone(e);
  const clone2 = structuredClone(clone);
  assertDOMException(clone, e);
  assertDOMException(clone2, e);
}

{
  // Clone a DOMException with a name
  const e = new DOMException('test', 'DataCloneError');
  const clone = structuredClone(e);
  const clone2 = structuredClone(clone);
  assertDOMException(clone, e);
  assertDOMException(clone2, e);
}

{
  // Clone an arbitrary object with a DOMException prototype
  const obj = {};
  Object.setPrototypeOf(obj, DOMException.prototype);
  const clone = structuredClone(obj);
  assert.strictEqual(clone instanceof DOMException, false);
}

{
  // Transfer a DOMException. DOMExceptions are not transferable.
  const e = new DOMException('test');
  assert.throws(() => {
    structuredClone(e, { transfer: [e] });
  }, {
    name: 'DataCloneError',
  });
}
