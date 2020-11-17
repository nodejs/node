'use strict';
const common = require('../../common');
const assert = require('assert');

const binding = require(`./build/${common.buildType}/test_subclass`);

class JSClass {
  superMethod(param) { return param + 1; }
  chainableMethod(param) { return param + '-1'; }
  value = 5;
}

class JSSubclassOfJSClass extends JSClass {
  subMethod(param) { return param - 1; }
  chainableMethod(param) { return '1-' + super.chainableMethod(param); }
}

const NativeClass = binding.NativeClass;

class JSSubclassOfNativeClass extends NativeClass {
  subMethod(param) { return param - 1; }
  chainableMethod(param) { return '1-' + super.chainableMethod(param); }
}

const NativeSubclassOfJSClass = binding.defineSubclass(JSClass);

const NativeSubClassOfNativeClass = binding.NativeSubclass;

// Testing pure-JS provides an easy test-of-the-test to make sure we expect the
// right things of the native class hierarchy.
test(JSClass, JSSubclassOfJSClass);
test(JSClass, NativeSubclassOfJSClass);
test(NativeClass, JSSubclassOfNativeClass);
test(NativeClass, NativeSubClassOfNativeClass);

function test(Superclass, Subclass) {
  const superItem = new Superclass();
  const subItem = new Subclass();

  assert.strictEqual(superItem.superMethod(42), 43);
  assert.strictEqual(subItem.superMethod(42), 43);
  assert.strictEqual(subItem.subMethod(42), 41);
  assert.strictEqual(superItem.chainableMethod('something'), 'something-1');
  assert.strictEqual(subItem.chainableMethod('something'), '1-something-1');
  assert.strictEqual(superItem.value, 5);
  assert.strictEqual(subItem.value, 5);
  assert(superItem instanceof Superclass);
  assert(subItem instanceof Subclass);
  assert(subItem instanceof Superclass);
  assert.strictEqual(Subclass.__proto__, Superclass);
}
