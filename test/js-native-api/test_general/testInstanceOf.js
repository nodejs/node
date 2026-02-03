'use strict';
const common = require('../../common');
const assert = require('assert');

// Addon is referenced through the eval expression in testFile
const addon = require(`./build/${common.buildType}/test_general`);

// We can only perform this test if we have a working Symbol.hasInstance
if (typeof Symbol !== 'undefined' && 'hasInstance' in Symbol &&
    typeof Symbol.hasInstance === 'symbol') {

  function compareToNative(theObject, theConstructor) {
    assert.strictEqual(
      addon.doInstanceOf(theObject, theConstructor),
      (theObject instanceof theConstructor),
    );
  }

  function MyClass() {}
  Object.defineProperty(MyClass, Symbol.hasInstance, {
    value: function(candidate) {
      return 'mark' in candidate;
    },
  });

  function MySubClass() {}
  MySubClass.prototype = new MyClass();

  let x = new MySubClass();
  let y = new MySubClass();
  x.mark = true;

  compareToNative(x, MySubClass);
  compareToNative(y, MySubClass);
  compareToNative(x, MyClass);
  compareToNative(y, MyClass);

  x = new MyClass();
  y = new MyClass();
  x.mark = true;

  compareToNative(x, MySubClass);
  compareToNative(y, MySubClass);
  compareToNative(x, MyClass);
  compareToNative(y, MyClass);
}
