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

function runAssertions(data, property, viaDefine, value1, value2, value3) {
  // Define the property for the first time
  setPropertyAndAssert(data, property, viaDefine, value1);
  // Update the property
  setPropertyAndAssert(data, property, viaDefine, value2);
  // Delete the property
  deletePropertyAndAssert(data, property);
  // Re-define the property
  setPropertyAndAssert(data, property, viaDefine, value3);
  // Delete the property again
  deletePropertyAndAssert(data, property);
}

const fun1 = () => 1;
const fun2 = () => 2;
const fun3 = () => 3;

// Assertions on: define property
runAssertions(global, 'toto', true, 1, 2, 3);
runAssertions(global, Symbol.for('toto'), true, 1, 2, 3);
runAssertions(global, 'tutu', true, fun1, fun2, fun3);
runAssertions(global, Symbol.for('tutu'), true, fun1, fun2, fun3);
runAssertions(global, 'tyty', false, fun1, 2, 3);
runAssertions(global, Symbol.for('tyty'), false, fun1, 2, 3);

// Assertions on: direct assignment
runAssertions(global, 'titi', false, 1, 2, 3);
runAssertions(global, Symbol.for('titi'), false, 1, 2, 3);
runAssertions(global, 'tata', false, fun1, fun2, fun3);
runAssertions(global, Symbol.for('tata'), false, fun1, fun2, fun3);
runAssertions(global, 'tztz', false, fun1, 2, 3);
runAssertions(global, Symbol.for('tztz'), false, fun1, 2, 3);

// Assertions on: define property from sandbox
vm.runInContext(
  "runAssertions(this, 'toto', true, 1, 2, 3)",
  vm.createContext({ runAssertions, fun1, fun2, fun3 })
);
vm.runInContext(
  "runAssertions(this, Symbol.for('toto'), true, 1, 2, 3)",
  vm.createContext({ runAssertions, fun1, fun2, fun3 })
);
vm.runInContext(
  "runAssertions(this, 'tutu', true, fun1, fun2, fun3)",
  vm.createContext({ runAssertions, fun1, fun2, fun3 })
);
vm.runInContext(
  "runAssertions(this, Symbol.for('tutu'), true, fun1, fun2, fun3)",
  vm.createContext({ runAssertions, fun1, fun2, fun3 })
);
vm.runInContext(
  "runAssertions(this, 'tyty', false, fun1, 2, 3)",
  vm.createContext({ runAssertions, fun1, fun2, fun3 })
);
vm.runInContext(
  "runAssertions(this, Symbol.for('tyty'), false, fun1, 2, 3)",
  vm.createContext({ runAssertions, fun1, fun2, fun3 })
);

// Assertions on: direct assignment from sandbox
vm.runInContext(
  "runAssertions(this, 'titi', false, 1, 2, 3)",
  vm.createContext({ runAssertions, fun1, fun2, fun3 })
);
vm.runInContext(
  "runAssertions(this, Symbol.for('titi'), false, 1, 2, 3)",
  vm.createContext({ runAssertions, fun1, fun2, fun3 })
);
vm.runInContext(
  "runAssertions(this, 'tata', false, fun1, fun2, fun3)",
  vm.createContext({ runAssertions, fun1, fun2, fun3 })
);
vm.runInContext(
  "runAssertions(this, Symbol.for('tata'), false, fun1, fun2, fun3)",
  vm.createContext({ runAssertions, fun1, fun2, fun3 })
);
vm.runInContext(
  "runAssertions(this, 'tztz', false, fun1, 2, 3)",
  vm.createContext({ runAssertions, fun1, fun2, fun3 })
);
vm.runInContext(
  "runAssertions(this, Symbol.for('tztz'), false, fun1, 2, 3)",
  vm.createContext({ runAssertions, fun1, fun2, fun3 })
);

// Helpers

// Set the property on data and assert it worked
function setPropertyAndAssert(data, property, viaDefine, value) {
  if (viaDefine) {
    Object.defineProperty(data, property, {
      enumerable: true,
      writable: true,
      value: value,
      configurable: true,
    });
  } else {
    data[property] = value;
  }
  assert.strictEqual(data[property], value);
  assert.ok(property in data);
  if (typeof property === 'string') {
    assert.ok(Object.getOwnPropertyNames(data).includes(property));
  } else {
    assert.ok(Object.getOwnPropertySymbols(data).includes(property));
  }
}

// Delete the property from data and assert it worked
function deletePropertyAndAssert(data, property) {
  delete data[property];
  assert.strictEqual(data[property], undefined);
  assert.ok(!(property in data));
  assert.ok(!Object.getOwnPropertyNames(data).includes(property));
  assert.ok(!Object.getOwnPropertySymbols(data).includes(property));
}
