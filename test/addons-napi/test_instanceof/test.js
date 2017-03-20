'use strict';
const fs = require('fs');

const common = require('../../common');
const assert = require('assert');

// addon is referenced through the eval expression in testFile
// eslint-disable-next-line no-unused-vars
const addon = require(`./build/${common.buildType}/test_instanceof`);
const path = require('path');

// The following assert functions are referenced by v8's unit tests
// See for instance deps/v8/test/mjsunit/instanceof.js
// eslint-disable-next-line no-unused-vars
function assertTrue(assertion) {
  return assert.strictEqual(true, assertion);
}

// eslint-disable-next-line no-unused-vars
function assertFalse(assertion) {
  assert.strictEqual(false, assertion);
}

// eslint-disable-next-line no-unused-vars
function assertEquals(leftHandSide, rightHandSide) {
  assert.strictEqual(leftHandSide, rightHandSide);
}

// eslint-disable-next-line no-unused-vars
function assertThrows(statement) {
  assert.throws(function() {
    eval(statement);
  }, Error);
}

function testFile(fileName) {
  const contents = fs.readFileSync(fileName, { encoding: 'utf8' });
  eval(contents.replace(/[(]([^\s(]+)\s+instanceof\s+([^)]+)[)]/g,
                        '(addon.doInstanceOf($1, $2))'));
}

testFile(
    path.join(path.resolve(__dirname, '..', '..', '..',
                           'deps', 'v8', 'test', 'mjsunit'),
              'instanceof.js'));
testFile(
    path.join(path.resolve(__dirname, '..', '..', '..',
                           'deps', 'v8', 'test', 'mjsunit'),
              'instanceof-2.js'));

// We can only perform this test if we have a working Symbol.hasInstance
if (typeof Symbol !== 'undefined' && 'hasInstance' in Symbol &&
    typeof Symbol.hasInstance === 'symbol') {

  function compareToNative(theObject, theConstructor) {
    assert.strictEqual(addon.doInstanceOf(theObject, theConstructor),
      (theObject instanceof theConstructor));
  }

  const MyClass = function MyClass() {};
  Object.defineProperty(MyClass, Symbol.hasInstance, {
    value: function(candidate) {
      return 'mark' in candidate;
    }
  });

  const MySubClass = function MySubClass() {};
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
