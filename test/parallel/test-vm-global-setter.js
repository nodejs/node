'use strict';
const common = require('../common');
const assert = require('assert');
const vm = require('vm');

const getSetSymbolReceivingFunction = Symbol('sym-1');
const getSetSymbolReceivingNumber = Symbol('sym-2');
const symbolReceivingNumber = Symbol('sym-3');
const unknownSymbolReceivingNumber = Symbol('sym-4');

const window = createWindow();

const descriptor1 = Object.getOwnPropertyDescriptor(
  window.globalProxy,
  'getSetPropReceivingFunction'
);
assert.strictEqual(typeof descriptor1.get, 'function');
assert.strictEqual(typeof descriptor1.set, 'function');
assert.strictEqual(descriptor1.configurable, true);

const descriptor2 = Object.getOwnPropertyDescriptor(
  window.globalProxy,
  'getSetPropReceivingNumber'
);
assert.strictEqual(typeof descriptor2.get, 'function');
assert.strictEqual(typeof descriptor2.set, 'function');
assert.strictEqual(descriptor2.configurable, true);

const descriptor3 = Object.getOwnPropertyDescriptor(
  window.globalProxy,
  'propReceivingNumber'
);
assert.strictEqual(descriptor3.value, 44);

const descriptor4 = Object.getOwnPropertyDescriptor(
  window.globalProxy,
  'unknownPropReceivingNumber'
);
assert.strictEqual(descriptor4, undefined);

const descriptor5 = Object.getOwnPropertyDescriptor(
  window.globalProxy,
  getSetSymbolReceivingFunction
);
assert.strictEqual(typeof descriptor5.get, 'function');
assert.strictEqual(typeof descriptor5.set, 'function');
assert.strictEqual(descriptor5.configurable, true);

const descriptor6 = Object.getOwnPropertyDescriptor(
  window.globalProxy,
  getSetSymbolReceivingNumber
);
assert.strictEqual(typeof descriptor6.get, 'function');
assert.strictEqual(typeof descriptor6.set, 'function');
assert.strictEqual(descriptor6.configurable, true);

const descriptor7 = Object.getOwnPropertyDescriptor(
  window.globalProxy,
  symbolReceivingNumber
);
assert.strictEqual(descriptor7.value, 48);

const descriptor8 = Object.getOwnPropertyDescriptor(
  window.globalProxy,
  unknownSymbolReceivingNumber
);
assert.strictEqual(descriptor8, undefined);

const descriptor9 = Object.getOwnPropertyDescriptor(
  window.globalProxy,
  'getSetPropThrowing'
);
assert.strictEqual(typeof descriptor9.get, 'function');
assert.strictEqual(typeof descriptor9.set, 'function');
assert.strictEqual(descriptor9.configurable, true);

const descriptor10 = Object.getOwnPropertyDescriptor(
  window.globalProxy,
  'nonWritableProp'
);
assert.strictEqual(descriptor10.value, 51);
assert.strictEqual(descriptor10.writable, false);

// Regression test for GH-42962. This assignment should not throw.
window.globalProxy.getSetPropReceivingFunction = () => {};
assert.strictEqual(window.globalProxy.getSetPropReceivingFunction, 42);

window.globalProxy.getSetPropReceivingNumber = 143;
assert.strictEqual(window.globalProxy.getSetPropReceivingNumber, 43);

window.globalProxy.propReceivingNumber = 144;
assert.strictEqual(window.globalProxy.propReceivingNumber, 144);

window.globalProxy.unknownPropReceivingNumber = 145;
assert.strictEqual(window.globalProxy.unknownPropReceivingNumber, 145);

window.globalProxy[getSetSymbolReceivingFunction] = () => {};
assert.strictEqual(window.globalProxy[getSetSymbolReceivingFunction], 46);

window.globalProxy[getSetSymbolReceivingNumber] = 147;
assert.strictEqual(window.globalProxy[getSetSymbolReceivingNumber], 47);

window.globalProxy[symbolReceivingNumber] = 148;
assert.strictEqual(window.globalProxy[symbolReceivingNumber], 148);

window.globalProxy[unknownSymbolReceivingNumber] = 149;
assert.strictEqual(window.globalProxy[unknownSymbolReceivingNumber], 149);

assert.throws(
  () => (window.globalProxy.getSetPropThrowing = 150),
  new Error('setter called')
);
assert.strictEqual(window.globalProxy.getSetPropThrowing, 50);

assert.throws(
  () => (window.globalProxy.nonWritableProp = 151),
  new TypeError('Cannot redefine property: nonWritableProp')
);
assert.strictEqual(window.globalProxy.nonWritableProp, 51);

function createWindow() {
  const obj = {};
  vm.createContext(obj);
  Object.defineProperty(obj, 'getSetPropReceivingFunction', {
    get: common.mustCall(() => 42),
    set: common.mustCall(),
    configurable: true,
  });
  Object.defineProperty(obj, 'getSetPropReceivingNumber', {
    get: common.mustCall(() => 43),
    set: common.mustCall(),
    configurable: true,
  });
  obj.propReceivingNumber = 44;
  Object.defineProperty(obj, getSetSymbolReceivingFunction, {
    get: common.mustCall(() => 46),
    set: common.mustCall(),
    configurable: true,
  });
  Object.defineProperty(obj, getSetSymbolReceivingNumber, {
    get: common.mustCall(() => 47),
    set: common.mustCall(),
    configurable: true,
  });
  obj[symbolReceivingNumber] = 48;
  Object.defineProperty(obj, 'getSetPropThrowing', {
    get: common.mustCall(() => 50),
    set: common.mustCall(() => {
      throw new Error('setter called');
    }),
    configurable: true,
  });
  Object.defineProperty(obj, 'nonWritableProp', {
    value: 51,
    writable: false,
  });

  obj.globalProxy = vm.runInContext('this', obj);

  return obj;
}
