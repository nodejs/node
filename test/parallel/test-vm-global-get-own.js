'use strict';
require('../common');
const assert = require('assert');
const vm = require('vm');

// These assertions check that we can set new keys to the global context,
// get them back and also list them via getOwnProperty* or in.
//
// Related to:
// - https://github.com/nodejs/node/issues/45983

const global = vm.runInContext('this', vm.createContext());

function runAssertions(property, viaDefine, value1, value2, value3) {
  // Define the property for the first time
  setPropertyAndAssert(property, viaDefine, value1);
  // Update the property
  setPropertyAndAssert(property, viaDefine, value2);
  // Delete the property
  deletePropertyAndAssert(property);
  // Re-define the property
  setPropertyAndAssert(property, viaDefine, value3);
  // Delete the property again
  deletePropertyAndAssert(property);
}

const fun1 = () => 1;
const fun2 = () => 2;
const fun3 = () => 3;

// Assertions on: define property
runAssertions('toto', true, 1, 2, 3);
runAssertions(Symbol.for('toto'), true, 1, 2, 3);
runAssertions('tutu', true, fun1, fun2, fun3);
runAssertions(Symbol.for('tutu'), true, fun1, fun2, fun3);
runAssertions('tyty', false, fun1, 2, 3);
runAssertions(Symbol.for('tyty'), false, fun1, 2, 3);

// Assertions on: direct assignment
runAssertions('titi', false, 1, 2, 3);
runAssertions(Symbol.for('titi'), false, 1, 2, 3);
runAssertions('tata', false, fun1, fun2, fun3);
runAssertions(Symbol.for('tata'), false, fun1, fun2, fun3);
runAssertions('tztz', false, fun1, 2, 3);
runAssertions(Symbol.for('tztz'), false, fun1, 2, 3);

// Helpers

// Set the property on global and assert it worked
function setPropertyAndAssert(property, viaDefine, value) {
  if (viaDefine) {
    Object.defineProperty(global, property, {
      enumerable: true,
      writable: true,
      value: value,
      configurable: true,
    });
  } else {
    global[property] = value;
  }
  assert.strictEqual(global[property], value);
  assert.ok(property in global);
  if (typeof property === 'string') {
    assert.ok(Object.getOwnPropertyNames(global).includes(property));
  } else {
    assert.ok(Object.getOwnPropertySymbols(global).includes(property));
  }
}

// Delete the property from global and assert it worked
function deletePropertyAndAssert(property) {
  delete global[property];
  assert.strictEqual(global[property], undefined);
  assert.ok(!(property in global));
  assert.ok(!Object.getOwnPropertyNames(global).includes(property));
  assert.ok(!Object.getOwnPropertySymbols(global).includes(property));
}
