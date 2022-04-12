// Copyright 2021 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

const {session, contextGroup, Protocol} =
    InspectorTest.start('Test for console formatting');

Protocol.Runtime.onConsoleAPICalled(({params: {args, type}}) => {
  InspectorTest.logObject(args, type);
});

async function test(expression) {
  InspectorTest.logMessage(`Testing ${expression}...`);
  const {result} = await Protocol.Runtime.evaluate({expression});
  if ('exceptionDetails' in result) {
    InspectorTest.logMessage(result.exceptionDetails);
  }
}

InspectorTest.runAsyncTestSuite([
  async function testFloatFormatter() {
    await Protocol.Runtime.enable();
    await test(`console.debug('%f', 3.1415)`);
    await test(`console.error('%f', '3e2')`);
    await test(`console.info('%f', Symbol('1.1'))`);
    await test(`console.log('%f', {toString() { return '42'; }})`);
    await test(
        `console.trace('%f', {[Symbol.toPrimitive]() { return 2.78; }})`);
    await test(`console.warn('%f', {toString() { throw new Error(); }})`);
    await Promise.all([
      Protocol.Runtime.discardConsoleEntries(),
      Protocol.Runtime.disable(),
    ]);
  },

  async function testIntegerFormatter() {
    await Protocol.Runtime.enable();
    await test(`console.debug('%d', 42)`);
    await test(`console.error('%i', '987654321')`);
    await test(`console.info('%d', Symbol('12345'))`);
    await test(`console.log('%i', {toString() { return '42'; }})`);
    await test(`console.trace('%d', {[Symbol.toPrimitive]() { return 256; }})`);
    await test(`console.warn('%i', {toString() { throw new Error(); }})`);
    await Promise.all([
      Protocol.Runtime.discardConsoleEntries(),
      Protocol.Runtime.disable(),
    ]);
  },

  async function testStringFormatter() {
    await Protocol.Runtime.enable();
    await test(`console.debug('%s', 42)`);
    await test(`console.error('%s', 'Test string')`);
    await test(`console.info('%s', Symbol('Test symbol'))`);
    await test(`console.log('%s', {toString() { return 'Test object'; }})`);
    await test(
        `console.trace('%s', {[Symbol.toPrimitive]() { return true; }})`);
    await test(`console.warn('%s', {toString() { throw new Error(); }})`);
    await Promise.all([
      Protocol.Runtime.discardConsoleEntries(),
      Protocol.Runtime.disable(),
    ]);
  },

  async function testOtherFormatters() {
    await Protocol.Runtime.enable();
    await test(`console.debug('%c', 'color:red')`);
    await test(`console.error('%o', {toString() { throw new Error(); }})`);
    await test(`console.info('%O', {toString() { throw new Error(); }})`);
    await test(
        `console.log('We have reached 100% of our users', 'with this!')`);
    await Promise.all([
      Protocol.Runtime.discardConsoleEntries(),
      Protocol.Runtime.disable(),
    ]);
  },

  async function testMultipleFormatters() {
    await Protocol.Runtime.enable();
    await test(`console.debug('%s%some Text%i', '', 'S', 1)`);
    await test(
        `console.error('%c%i%c%s', 'color:red', 42, 'color:green', 'Message!')`);
    await test(
        `console.info('%s', {toString() { return '%i% %s %s'; }}, {toString() { return '100'; }}, 'more', 'arguments')`);
    await test(
        `console.log('%s %s', {toString() { return 'Too %s %s'; }}, 'many', 'specifiers')`);
    await test(
        `console.trace('%s %f', {toString() { return '%s'; }}, {[Symbol.toPrimitive]() { return 'foo'; }}, 1, 'Test')`);
    await Promise.all([
      Protocol.Runtime.discardConsoleEntries(),
      Protocol.Runtime.disable(),
    ]);
  },

  async function testAssert() {
    await Protocol.Runtime.enable();
    await test(
        `console.assert(true, '%s', {toString() { throw new Error(); }})`);
    await test(
        `console.assert(false, '%s %i', {toString() { return '%s'; }}, {[Symbol.toPrimitive]() { return 1; }}, 1, 'Test')`);
    await test(
        `console.assert(false, '%s', {toString() { throw new Error(); }})`);
    await Promise.all([
      Protocol.Runtime.discardConsoleEntries(),
      Protocol.Runtime.disable(),
    ]);
  },

  async function testGroup() {
    await Protocol.Runtime.enable();
    await test(`console.group('%s', {toString() { throw new Error(); }})`);
    await test(
        `console.group('%s%i', 'Gruppe', {[Symbol.toPrimitive]() { return 1; }})`);
    await test(`console.groupEnd()`);
    await Promise.all([
      Protocol.Runtime.discardConsoleEntries(),
      Protocol.Runtime.disable(),
    ]);
  },

  async function testGroupCollapsed() {
    await Protocol.Runtime.enable();
    await test(
        `console.groupCollapsed('%d', {toString() { throw new Error(); }})`);
    await test(
        `console.groupCollapsed('%s%f', {[Symbol.toPrimitive]() { return 'Gruppe'; }}, 3.1415)`);
    await test(`console.groupEnd()`);
    await Promise.all([
      Protocol.Runtime.discardConsoleEntries(),
      Protocol.Runtime.disable(),
    ]);
  },

  async function testNonStandardFormatSpecifiers() {
    await Protocol.Runtime.enable();
    await test(
        `console.log('%_ %s', {toString() { throw new Error(); }}, {toString() { return 'foo'; }})`);
    await test(`console.log('%%s', {toString() { throw new Error(); }})`);
    await Promise.all([
      Protocol.Runtime.discardConsoleEntries(),
      Protocol.Runtime.disable(),
    ]);
  }
]);
