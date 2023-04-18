'use strict';
// Flags: --expose-gc
//
// Testing API calls for Node-API references.
// We compare their behavior between Node-API version 8 and later.
// In version 8 references can be created only for object, function,
// and symbol types, while in newer versions they can be created for
// any value type.
//
const { gcUntil, buildType } = require('../../common');
const assert = require('assert');
const addon_v8 = require(`./build/${buildType}/test_reference_obj_only`);
const addon_new = require(`./build/${buildType}/test_reference_all_types`);

async function runTests(addon, isVersion8) {
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
      { value: undefinedValue, canBeWeak: false, canBeRefV8: false },
      { value: nullValue, canBeWeak: false, canBeRefV8: false },
      { value: booleanValue, canBeWeak: false, canBeRefV8: false },
      { value: numberValue, canBeWeak: false, canBeRefV8: false },
      { value: stringValue, canBeWeak: false, canBeRefV8: false },
      { value: symbolValue, canBeWeak: false, canBeRefV8: true },
      { value: objectValue, canBeWeak: true, canBeRefV8: true },
      { value: functionValue, canBeWeak: true, canBeRefV8: true },
      { value: externalValue, canBeWeak: true, canBeRefV8: true },
      { value: bigintValue, canBeWeak: false, canBeRefV8: false },
    ];

    // Go over all values of different types, create strong ref values for
    // them, read the stored values, and check how the ref count works.
    for (const entry of allEntries) {
      if (!isVersion8 || entry.canBeRefV8) {
        const index = addon.createRef(entry.value);
        const refValue = addon.getRefValue(index);
        assert.strictEqual(entry.value, refValue);
        assert.strictEqual(addon.ref(index), 2);
        assert.strictEqual(addon.unref(index), 1);
        assert.strictEqual(addon.unref(index), 0);
      } else {
        assert.throws(() => { addon.createRef(entry.value); },
                      {
                        name: 'Error',
                        message: 'Invalid argument',
                      });
      }
    }

    // When the reference count is zero, then object types become weak pointers
    // and other types are released.
    // Here we know that the GC is not run yet because the values are
    // still in the allEntries array.
    allEntries.forEach((entry, index) => {
      if (!isVersion8 || entry.canBeRefV8) {
        if (entry.canBeWeak) {
          assert.strictEqual(addon.getRefValue(index), entry.value);
        } else {
          assert.strictEqual(addon.getRefValue(index), undefined);
        }
      }
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
  allEntries.forEach((entry, index) => {
    if (!isVersion8 || entry.canBeRefV8) {
      assert.strictEqual(addon.getRefValue(index), undefined);
      addon.deleteRef(index);
    }
  });
}

runTests(addon_v8, /* isVersion8 */ true);
runTests(addon_new, /* isVersion8 */ false);
