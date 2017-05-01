'use strict';

// https://heycam.github.io/webidl/#es-namespaces
// https://console.spec.whatwg.org/#console-namespace
// https://github.com/w3c/web-platform-tests/blob/40e451c/console/console-is-a-namespace.any.js

const common = require('../common');
const assert = require('assert');
const { test, assert_equals, assert_true, assert_false } = common.WPT;

assert.doesNotThrow(() => {
  global.console = global.console;
});

// Tests above are not from WPT.
// Tests below are from WPT.
// avoid to modify them

/* eslint-disable */
/* The following tests are copied from. Do not modify. */
/* WPT Refs:
   https://github.com/w3c/web-platform-tests/blob/40e451c/console/console-is-a-namespace.any.js
   License: http://www.w3.org/Consortium/Legal/2008/04-testsuite-copyright.html
*/

test(() => {
  assert_true(global.hasOwnProperty('console'));
}, 'console exists on the global object');

test(() => {
  assert_true(global.hasOwnProperty('console'));
}, 'console exists on the global object');

test(() => {
  const propDesc = Object.getOwnPropertyDescriptor(global, 'console');
  assert_equals(propDesc.writable, true, 'must be writable');
  assert_equals(propDesc.enumerable, false, 'must not be enumerable');
  assert_equals(propDesc.configurable, true, 'must be configurable');
  assert_equals(propDesc.value, console, 'must have the right value');
}, 'console has the right property descriptors');

test(() => {
  assert_false('Console' in global);
}, 'Console (uppercase, as if it were an interface) must not exist');

test(() => {
  const prototype1 = Object.getPrototypeOf(console);
  const prototype2 = Object.getPrototypeOf(prototype1);
  // assert_equals(Object.getOwnPropertyNames(prototype1).length, 0,
  //                'The [[Prototype]] must have no properties');
  assert_equals(prototype2, Object.prototype,
                'The [[Prototype]]\'s [[Prototype]] must be Object Prototype');

}, 'The prototype chain must be correct');

/* eslint-enable */
