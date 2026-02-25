// Flags: --experimental-ffi
'use strict';
const { skipIfFFIMissing } = require('../common');
skipIfFFIMissing();

const { UnsafePointer } = require('node:ffi');
const { suite, test } = require('node:test');

suite('UnsafePointer.create()', () => {
  test('creates a pointer object', (t) => {
    const ptr = UnsafePointer.create(0x12345678n);

    t.assert.notStrictEqual(ptr, null);
    t.assert.strictEqual(typeof ptr, 'object');
    t.assert.strictEqual(Object.getPrototypeOf(ptr), null);
    t.assert.strictEqual(Object.keys(ptr).length, 0);
    t.assert.strictEqual(Object.isFrozen(ptr), true);
    t.assert.strictEqual(ptr.address, undefined);
  });

  test('throws when the input is not a BigInt', (t) => {
    t.assert.throws(() => {
      UnsafePointer.create(0x12345678);
    }, TypeError);

    t.assert.throws(() => {
      UnsafePointer.create('0x12345678');
    }, TypeError);

    t.assert.throws(() => {
      UnsafePointer.create(null);
    }, TypeError);

    t.assert.throws(() => {
      UnsafePointer.create(undefined);
    }, TypeError);
  });
});

suite('UnsafePointer.equals()', () => {
  test('returns true for pointers with the same address', (t) => {
    const ptr1 = UnsafePointer.create(0x1000n);
    const ptr2 = UnsafePointer.create(0x1000n);

    t.assert.strictEqual(UnsafePointer.equals(ptr1, ptr2), true);
  });

  test('returns false for pointers with different addresses', (t) => {
    const ptr1 = UnsafePointer.create(0x1000n);
    const ptr2 = UnsafePointer.create(0x2000n);

    t.assert.strictEqual(UnsafePointer.equals(ptr1, ptr2), false);
  });

  test('works with offset pointers', (t) => {
    const ptr = UnsafePointer.create(0x1000n);
    const offsetPtr1 = UnsafePointer.offset(ptr, 16);
    const offsetPtr2 = UnsafePointer.offset(ptr, 16);
    const offsetPtr3 = UnsafePointer.offset(ptr, 32);
    const offsetPtr4 = UnsafePointer.offset(ptr, 0);

    t.assert.strictEqual(UnsafePointer.equals(offsetPtr1, offsetPtr2), true);
    t.assert.strictEqual(UnsafePointer.equals(offsetPtr1, offsetPtr3), false);
    t.assert.strictEqual(UnsafePointer.equals(ptr, offsetPtr4), true);
  });

  test('throws when the first argument is not a pointer object', (t) => {
    const ptr = UnsafePointer.create(0x1000n);

    t.assert.throws(() => {
      UnsafePointer.equals(0x1000n, ptr);
    }, TypeError);

    t.assert.throws(() => {
      UnsafePointer.equals({ address: 0x1000n }, ptr);
    }, TypeError);
  });

  test('throws when the second argument is not a pointer object', (t) => {
    const ptr = UnsafePointer.create(0x1000n);

    t.assert.throws(() => {
      UnsafePointer.equals(ptr, 0x1000n);
    }, TypeError);

    t.assert.throws(() => {
      UnsafePointer.equals(ptr, { address: 0x1000n });
    }, TypeError);
  });
});

suite('UnsafePointer.offset()', () => {
  test('creates a pointer object from the offset', (t) => {
    const ptr = UnsafePointer.create(0x1000n);
    const offsetPtr = UnsafePointer.offset(ptr, 16);

    t.assert.notStrictEqual(offsetPtr, null);
    t.assert.strictEqual(typeof offsetPtr, 'object');
    t.assert.strictEqual(Object.getPrototypeOf(offsetPtr), null);
    t.assert.strictEqual(Object.keys(offsetPtr).length, 0);
    t.assert.strictEqual(Object.isFrozen(offsetPtr), true);
    t.assert.strictEqual(offsetPtr.address, undefined);

    const value = UnsafePointer.value(offsetPtr);
    t.assert.strictEqual(value, 0x1010n);
  });

  test('works with BigInt offset', (t) => {
    const ptr = UnsafePointer.create(0x1000n);
    const offsetPtr = UnsafePointer.offset(ptr, 32n);

    const value = UnsafePointer.value(offsetPtr);
    t.assert.strictEqual(value, 0x1020n);
  });

  test('works with negative offset', (t) => {
    const ptr = UnsafePointer.create(0x1000n);
    const offsetPtr = UnsafePointer.offset(ptr, -16);

    const value = UnsafePointer.value(offsetPtr);
    t.assert.strictEqual(value, 0xff0n);
  });

  test('throws when the first argument is not a pointer object', (t) => {
    t.assert.throws(() => {
      UnsafePointer.offset(0x1000n, 16);
    }, TypeError);

    t.assert.throws(() => {
      UnsafePointer.offset({ address: 0x1000n }, 16);
    }, TypeError);

    t.assert.throws(() => {
      UnsafePointer.offset(null, 16);
    }, TypeError);
  });

  test('throws when the offset argument is invalid', (t) => {
    const ptr = UnsafePointer.create(0x1000n);

    t.assert.throws(() => {
      UnsafePointer.offset(ptr, 'invalid');
    }, TypeError);

    t.assert.throws(() => {
      UnsafePointer.offset(ptr, null);
    }, TypeError);

    t.assert.throws(() => {
      UnsafePointer.offset(ptr, undefined);
    }, TypeError);
  });
});

suite('UnsafePointer.value()', () => {
  test('returns the address held in a pointer object', (t) => {
    const address = 0x12345678n;
    const ptr = UnsafePointer.create(address);
    const value = UnsafePointer.value(ptr);
    t.assert.strictEqual(value, address);
  });

  test('throws when the input is not a pointer object', (t) => {
    t.assert.throws(() => {
      UnsafePointer.value(0x12345678n);
    }, TypeError);

    t.assert.throws(() => {
      UnsafePointer.value({ address: 0x12345678n });
    }, TypeError);

    t.assert.throws(() => {
      UnsafePointer.value(null);
    }, TypeError);

    t.assert.throws(() => {
      UnsafePointer.value(undefined);
    }, TypeError);
  });
});
