'use strict';
// Flags: --expose-gc
//
// Testing API calls for references to all value types.
// This test uses NAPI_MODULE_INIT macro to initialize module.
//
const { gcUntil, buildType } = require('../../common');
const assert = require('assert');
const addon = require(`./build/${buildType}/test_reference_all_types`);

async function runTests() {
  let allEntries = [];

  (() => {
    // Create values of all napi_valuetype types.
    const undefinedValue = undefined;
    const nullValue = null;
    const booleanValue = false;
    const numberValue = 42;
    const stringValue = 'test_string';
    const symbolValue = Symbol.for('test_symbol');
    const objectValue = { x: 1, y: 2 };
    const functionValue = (x, y) => x + y;
    const externalValue = addon.createExternal();
    const bigintValue = 9007199254740991n;

    allEntries = [
      { value: undefinedValue, canBeWeak: false },
      { value: nullValue, canBeWeak: false },
      { value: booleanValue, canBeWeak: false },
      { value: numberValue, canBeWeak: false },
      { value: stringValue, canBeWeak: false },
      { value: symbolValue, canBeWeak: false },
      { value: objectValue, canBeWeak: true },
      { value: functionValue, canBeWeak: true },
      { value: externalValue, canBeWeak: true },
      { value: bigintValue, canBeWeak: false },
    ];

    // Go over all values of different types, create strong ref values for
    // them, read the stored values, and check how the ref count works.
    for (const entry of allEntries) {
      const index = addon.createRef(entry.value);
      const refValue = addon.getRefValue(index);
      assert.strictEqual(entry.value, refValue);
      assert.strictEqual(addon.ref(index), 2);
      assert.strictEqual(addon.unref(index), 1);
      assert.strictEqual(addon.unref(index), 0);
    }

    // The references become weak pointers when the ref count is 0.
    // Here we know that the GC is not run yet because the values are
    // still in the allEntries array.
    allEntries.forEach((entry, index) => {
      assert.strictEqual(addon.getRefValue(index), entry.value);
      // Set to undefined to allow GC collect the value.
      entry.value = undefined;
    });

    // To check that GC pass is done.
    const objWithFinalizer = {};
    addon.addFinalizer(objWithFinalizer);
  })();

  assert.strictEqual(addon.getFinalizeCount(), 0);
  await gcUntil('Wait until a finalizer is called',
                () => (addon.getFinalizeCount() === 1));

  // Create and call finalizer again to make sure that we had another GC pass.
  (() => {
    const objWithFinalizer = {};
    addon.addFinalizer(objWithFinalizer);
  })();
  await gcUntil('Wait until a finalizer is called again',
                () => (addon.getFinalizeCount() === 2));

  // After GC and finalizers run, all values that support weak reference
  // semantic must return undefined value.
  // It also includes the value at index 0 because it is the undefined value.
  // Other value types are not collected by GC.
  allEntries.forEach((entry, index) => {
    if (entry.canBeWeak || index === 0) {
      assert.strictEqual(addon.getRefValue(index), undefined);
    } else {
      assert.notStrictEqual(addon.getRefValue(index), undefined);
    }
    addon.deleteRef(index);
  });
}
runTests();
