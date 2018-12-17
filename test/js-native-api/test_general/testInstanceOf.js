'use strict';
const fs = require('fs');

const common = require('../../common');
const assert = require('assert');

// Addon is referenced through the eval expression in testFile
const addon = require(`./build/${common.buildType}/test_general`);
const path = require('path');

// This test depends on a number of V8 tests.
const v8TestsDir = path.resolve(__dirname, '..', '..', '..', 'deps', 'v8',
                                'test', 'mjsunit');
const v8TestsDirExists = fs.existsSync(v8TestsDir);

// The following assert functions are referenced by v8's unit tests
// See for instance deps/v8/test/mjsunit/instanceof.js
// eslint-disable-next-line no-unused-vars
function assertTrue(assertion) {
  return assert.strictEqual(assertion, true);
}

// eslint-disable-next-line no-unused-vars
function assertFalse(assertion) {
  assert.strictEqual(assertion, false);
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
  try {
    const contents = fs.readFileSync(fileName, { encoding: 'utf8' });
    eval(contents.replace(/[(]([^\s(]+)\s+instanceof\s+([^)]+)[)]/g,
                          '(addon.doInstanceOf($1, $2))'));
  } catch (err) {
    // This test depends on V8 test files, which may not exist in downloaded
    // archives. Emit a warning if the tests cannot be found instead of failing.
    if (err.code === 'ENOENT' && !v8TestsDirExists)
      process.emitWarning(`test file ${fileName} does not exist.`);
    else
      throw err;
  }
}

testFile(path.join(v8TestsDir, 'instanceof.js'));
testFile(path.join(v8TestsDir, 'instanceof-2.js'));

// We can only perform this test if we have a working Symbol.hasInstance
if (typeof Symbol !== 'undefined' && 'hasInstance' in Symbol &&
    typeof Symbol.hasInstance === 'symbol') {

  function compareToNative(theObject, theConstructor) {
    assert.strictEqual(
      addon.doInstanceOf(theObject, theConstructor),
      (theObject instanceof theConstructor)
    );
  }

  function MyClass() {}
  Object.defineProperty(MyClass, Symbol.hasInstance, {
    value: function(candidate) {
      return 'mark' in candidate;
    }
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
