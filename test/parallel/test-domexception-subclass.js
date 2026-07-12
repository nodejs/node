'use strict';

require('../common');
const assert = require('assert');

class MyDOMException extends DOMException {
  ownProp;
  #reason;

  constructor() {
    super('my message', 'NotFoundError');
    this.ownProp = 'bar';
    this.#reason = 'hello';
  }

  get reason() {
    return this.#reason;
  }
}

const myException = new MyDOMException();
// Verifies the prototype chain
assert(myException instanceof MyDOMException);
assert(myException instanceof DOMException);
assert(myException instanceof Error);
// Verifies [[ErrorData]]
assert(Error.isError(myException));

// Verifies subclass properties
assert(Object.hasOwn(myException, 'ownProp'));
assert(!Object.hasOwn(myException, 'reason'));
assert.strictEqual(myException.reason, 'hello');

// Verifies error properties
assert.strictEqual(myException.name, 'NotFoundError');
assert.strictEqual(myException.code, 8);
assert.strictEqual(myException.message, 'my message');
assert.strictEqual(typeof myException.stack, 'string');

// Verify structuredClone only copies known error properties.
const cloned = structuredClone(myException);
assert(!(cloned instanceof MyDOMException));
assert(cloned instanceof DOMException);
assert(cloned instanceof Error);
assert(Error.isError(cloned));

// Verify custom properties
assert(!Object.hasOwn(cloned, 'ownProp'));
assert.strictEqual(cloned.reason, undefined);

// Verify cloned error properties
assert.strictEqual(cloned.name, 'NotFoundError');
assert.strictEqual(cloned.code, 8);
assert.strictEqual(cloned.message, 'my message');
assert.strictEqual(cloned.stack, myException.stack);
