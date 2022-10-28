'use strict';
// Flags: --expose-gc

const { gcUntil, buildType } = require('../../common');
const assert = require('assert');

// Testing API calls for references to only object, function, and symbol types.
// This is the reference behavior before when the
// napi_feature_reference_all_types feature is not enabled.
// This test uses NAPI_MODULE_INIT macro to initialize module.
const addon = require(`./build/${buildType}/test_init_reference_obj_only`);

async function runTests() {
  let entryCount = 0;
  let refIndexes = [];
  let symbolValueIndex = -1;

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

    // true if the value can be a ref.
    const allEntries = [
      [ undefinedValue, false ],
      [ nullValue, false ],
      [ booleanValue, false ],
      [ numberValue, false ],
      [ stringValue, false ],
      [ symbolValue, true ],
      [ objectValue, true ],
      [ functionValue, true ],
      [ externalValue, true ],
      [ bigintValue, false ],
    ];
    entryCount = allEntries.length;
    symbolValueIndex = allEntries.findIndex(entry => entry[0] === symbolValue);

    // Go over all values of different types, create strong ref values for
    // them, read the stored values, and check how the ref count works.
    for (const entry of allEntries) {
      const value = entry[0];
      if (entry[1]) {
        const index = addon.createRef(value);
        const refValue = addon.getRefValue(index);
        assert.strictEqual(value, refValue);
        assert.strictEqual(addon.ref(index), 2);
        assert.strictEqual(addon.unref(index), 1);
        assert.strictEqual(addon.unref(index), 0);
      } else {
        assert.throws(() => addon.createRef(value));
      }
    }

    // The references become weak pointers when the ref count is 0.
    // The old reference were supported for objects, functions, and symbols.
    // Here we know that the GC is not run yet because the values are
    // still in the allValues array.
    for (let i = 0; i < entryCount; ++i) {
      if (allEntries[i][1]) {
        assert.strictEqual(addon.getRefValue(i), allEntries[i][0]);
        refIndexes.push(i);
      }
    }

    const objWithFinalizer = {};
    addon.addFinalizer(objWithFinalizer);
  })();

  assert.strictEqual(addon.getFinalizeCount(), 0);
  await gcUntil('Wait until a finalizer is called',
                () => (addon.getFinalizeCount() === 1));

  // Create and call finalizer again to make sure that all finalizers are run.
  (() => {
    const objWithFinalizer = {};
    addon.addFinalizer(objWithFinalizer);
  })();

  await gcUntil('Wait until a finalizer is called again',
                () => (addon.getFinalizeCount() === 1));

  // After GC and finalizers run, all references with refCount==0 must return
  // undefined value.
  for (const index of refIndexes) {
    const refValue = addon.getRefValue(index);
    // Symbols do not support the weak semantic
    if (symbolValueIndex !== 5) {
      assert.strictEqual(refValue, undefined);
    }
    addon.deleteRef(index);
  }
}
runTests();
