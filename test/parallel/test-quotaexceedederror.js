'use strict';

require('../common');
const assert = require('assert');

// Verify QuotaExceededError is a global.
assert.strictEqual(typeof QuotaExceededError, 'function');

// Prototype chain.
{
  const err = new QuotaExceededError('test');
  assert(err instanceof QuotaExceededError);
  assert(err instanceof DOMException);
  assert(err instanceof Error);
  assert(Error.isError(err));
}

// Default properties.
{
  const err = new QuotaExceededError();
  assert.strictEqual(err.name, 'QuotaExceededError');
  assert.strictEqual(err.message, '');
  assert.strictEqual(err.code, 22);
  assert.strictEqual(err.quota, null);
  assert.strictEqual(err.requested, null);
  assert.strictEqual(typeof err.stack, 'string');
}

// Custom message.
{
  const err = new QuotaExceededError('out of space');
  assert.strictEqual(err.message, 'out of space');
  assert.strictEqual(err.name, 'QuotaExceededError');
  assert.strictEqual(err.code, 22);
}

// Options with quota and requested.
{
  const err = new QuotaExceededError('full', { quota: 100, requested: 200 });
  assert.strictEqual(err.quota, 100);
  assert.strictEqual(err.requested, 200);
}

// Options with only quota.
{
  const err = new QuotaExceededError('full', { quota: 50 });
  assert.strictEqual(err.quota, 50);
  assert.strictEqual(err.requested, null);
}

// Options with only requested.
{
  const err = new QuotaExceededError('full', { requested: 75 });
  assert.strictEqual(err.quota, null);
  assert.strictEqual(err.requested, 75);
}

// Options with zero values.
{
  const err = new QuotaExceededError('full', { quota: 0, requested: 0 });
  assert.strictEqual(err.quota, 0);
  assert.strictEqual(err.requested, 0);
}

// Equal quota and requested is valid.
{
  const err = new QuotaExceededError('full', { quota: 100, requested: 100 });
  assert.strictEqual(err.quota, 100);
  assert.strictEqual(err.requested, 100);
}

// Numeric string coercion.
{
  const err = new QuotaExceededError('full', { quota: '10', requested: '20' });
  assert.strictEqual(err.quota, 10);
  assert.strictEqual(err.requested, 20);
}

// Validation: negative quota.
assert.throws(
  () => new QuotaExceededError('test', { quota: -1 }),
  { name: 'RangeError' },
);

// Validation: negative requested.
assert.throws(
  () => new QuotaExceededError('test', { requested: -1 }),
  { name: 'RangeError' },
);

// Validation: requested < quota.
assert.throws(
  () => new QuotaExceededError('test', { quota: 100, requested: 50 }),
  { name: 'RangeError' },
);

// Validation: NaN quota.
assert.throws(
  () => new QuotaExceededError('test', { quota: NaN }),
  { name: 'TypeError' },
);

// Validation: Infinity quota.
assert.throws(
  () => new QuotaExceededError('test', { quota: Infinity }),
  { name: 'TypeError' },
);

// Validation: NaN requested.
assert.throws(
  () => new QuotaExceededError('test', { requested: NaN }),
  { name: 'TypeError' },
);

// Validation: Infinity requested.
assert.throws(
  () => new QuotaExceededError('test', { requested: Infinity }),
  { name: 'TypeError' },
);

// Validation: -Infinity quota.
assert.throws(
  () => new QuotaExceededError('test', { quota: -Infinity }),
  { name: 'TypeError' },
);

// Null/undefined options are allowed.
{
  const e1 = new QuotaExceededError('test', null);
  assert.strictEqual(e1.quota, null);
  assert.strictEqual(e1.requested, null);

  const e2 = new QuotaExceededError('test', undefined);
  assert.strictEqual(e2.quota, null);
  assert.strictEqual(e2.requested, null);
}

// DOMException backwards compat: new DOMException('msg', 'QuotaExceededError')
// still works and returns code 22, but is not instanceof QuotaExceededError.
{
  const err = new DOMException('msg', 'QuotaExceededError');
  assert.strictEqual(err.code, 22);
  assert.strictEqual(err.name, 'QuotaExceededError');
  assert(!(err instanceof QuotaExceededError));
}

// SymbolToStringTag.
{
  const err = new QuotaExceededError('test');
  assert.strictEqual(Object.prototype.toString.call(err), '[object QuotaExceededError]');
}

// Property descriptors for quota/requested are enumerable and configurable.
{
  const proto = QuotaExceededError.prototype;
  for (const prop of ['quota', 'requested']) {
    const desc = Object.getOwnPropertyDescriptor(proto, prop);
    assert.strictEqual(desc.enumerable, true);
    assert.strictEqual(desc.configurable, true);
    assert.strictEqual(typeof desc.get, 'function');
  }
}

// Getter throws on invalid this.
{
  const proto = QuotaExceededError.prototype;
  assert.throws(
    () => Object.getOwnPropertyDescriptor(proto, 'quota').get.call({}),
    { code: 'ERR_INVALID_THIS' },
  );
  assert.throws(
    () => Object.getOwnPropertyDescriptor(proto, 'requested').get.call({}),
    { code: 'ERR_INVALID_THIS' },
  );
}

// structuredClone preserves properties.
{
  const original = new QuotaExceededError('cloned', { quota: 42, requested: 50 });
  const cloned = structuredClone(original);
  assert(cloned instanceof QuotaExceededError);
  assert(cloned instanceof DOMException);
  assert(cloned instanceof Error);
  assert.strictEqual(cloned.name, 'QuotaExceededError');
  assert.strictEqual(cloned.message, 'cloned');
  assert.strictEqual(cloned.code, 22);
  assert.strictEqual(cloned.quota, 42);
  assert.strictEqual(cloned.requested, 50);
  assert.strictEqual(cloned.stack, original.stack);
}

// structuredClone with null quota/requested.
{
  const original = new QuotaExceededError('defaults');
  const cloned = structuredClone(original);
  assert(cloned instanceof QuotaExceededError);
  assert.strictEqual(cloned.quota, null);
  assert.strictEqual(cloned.requested, null);
}
