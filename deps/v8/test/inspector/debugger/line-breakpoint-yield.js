// Copyright 2022 the V8 project authors. All rights reserved.
// Use  of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

let {session, contextGroup, Protocol} =
    InspectorTest.start('Line breakpoints for yield');

// clang-format off
const url = 'line-breakpoint-yield.js';
contextGroup.addScript(`
var obj = {
  foo() {
    debugger;
  },

  *barGenerator() {
    yield this.foo();
  },

  *bazGenerator() {
yield this.foo();
  },

  async* barAsyncGenerator() {
    yield this.foo();
  },

  async* bazAsyncGenerator() {
yield this.foo();
  },

  *minifiedGenerator(){yield this.foo();}
};
`, 0, 0, url);
// clang-format on

session.setupScriptMap();
InspectorTest.runAsyncTestSuite([
  async function testYieldInGeneratorWithLeadingWhitespace() {
    await Promise.all([
      Protocol.Debugger.enable(),
      Protocol.Runtime.enable(),
    ]);
    InspectorTest.log(
        'Setting breakpoint on `yield this.foo()` in `obj.barGenerator`');
    const {result: {breakpointId}} =
        await Protocol.Debugger.setBreakpointByUrl({
          url,
          lineNumber: 7,
          columnNumber: 0,
        });
    InspectorTest.log('Calling `obj.barGenerator().next()`');
    const pausedPromise = Protocol.Debugger.oncePaused();
    const evalPromise = Protocol.Runtime.evaluate({
      expression: 'obj.barGenerator().next()',
    });
    const {params: {hitBreakpoints}} = await pausedPromise;
    if (hitBreakpoints?.length === 1 && hitBreakpoints[0] === breakpointId) {
      InspectorTest.log('Hit breakpoint before calling into `this.foo`');
    } else {
      InspectorTest.log('Missed breakpoint before calling into `this.foo`');
    }
    await Promise.all([
      Protocol.Debugger.removeBreakpoint({breakpointId}),
      Protocol.Debugger.disable(),
      evalPromise,
      Protocol.Runtime.disable(),
    ]);
  },

  async function testYieldInGeneratorWithoutLeadingWhitespace() {
    await Promise.all([
      Protocol.Debugger.enable(),
      Protocol.Runtime.enable(),
    ]);
    InspectorTest.log(
        'Setting breakpoint on `await this.foo()` in `obj.bazGenerator`');
    const {result: {breakpointId}} =
        await Protocol.Debugger.setBreakpointByUrl({
          url,
          lineNumber: 11,
          columnNumber: 0,
        });
    InspectorTest.log('Calling `obj.bazGenerator().next()`');
    const pausedPromise = Protocol.Debugger.oncePaused();
    const evalPromise = Protocol.Runtime.evaluate({
      expression: 'obj.bazGenerator().next()',
    });
    const {params: {hitBreakpoints}} = await pausedPromise;
    if (hitBreakpoints?.length === 1 && hitBreakpoints[0] === breakpointId) {
      InspectorTest.log('Hit breakpoint before calling into `this.foo`');
    } else {
      InspectorTest.log('Missed breakpoint before calling into `this.foo`');
    }
    await Promise.all([
      Protocol.Debugger.removeBreakpoint({breakpointId}),
      Protocol.Debugger.disable(),
      evalPromise,
      Protocol.Runtime.disable(),
    ]);
  },

  async function testYieldInAsyncGeneratorWithLeadingWhitespace() {
    await Promise.all([
      Protocol.Debugger.enable(),
      Protocol.Runtime.enable(),
    ]);
    InspectorTest.log(
        'Setting breakpoint on `yield this.foo()` in `obj.barAsyncGenerator`');
    const {result: {breakpointId}} =
        await Protocol.Debugger.setBreakpointByUrl({
          url,
          lineNumber: 15,
          columnNumber: 0,
        });
    InspectorTest.log('Calling `obj.barAsyncGenerator().next()`');
    const pausedPromise = Protocol.Debugger.oncePaused();
    const evalPromise = Protocol.Runtime.evaluate({
      expression: 'obj.barAsyncGenerator().next()',
      awaitPromise: true,
    });
    const {params: {hitBreakpoints}} = await pausedPromise;
    if (hitBreakpoints?.length === 1 && hitBreakpoints[0] === breakpointId) {
      InspectorTest.log('Hit breakpoint before calling into `this.foo`');
    } else {
      InspectorTest.log('Missed breakpoint before calling into `this.foo`');
    }
    await Promise.all([
      Protocol.Debugger.removeBreakpoint({breakpointId}),
      Protocol.Debugger.disable(),
      evalPromise,
      Protocol.Runtime.disable(),
    ]);
  },

  async function testYieldInAsyncGeneratorWithoutLeadingWhitespace() {
    await Promise.all([
      Protocol.Debugger.enable(),
      Protocol.Runtime.enable(),
    ]);
    InspectorTest.log(
        'Setting breakpoint on `yield this.foo()` in `obj.bazAsyncGenerator`');
    const {result: {breakpointId}} =
        await Protocol.Debugger.setBreakpointByUrl({
          url,
          lineNumber: 19,
          columnNumber: 0,
        });
    InspectorTest.log('Calling `obj.bazAsyncGenerator().next()`');
    const pausedPromise = Protocol.Debugger.oncePaused();
    const evalPromise = Protocol.Runtime.evaluate({
      expression: 'obj.bazAsyncGenerator().next()',
      awaitPromise: true,
    });
    const {params: {hitBreakpoints}} = await pausedPromise;
    if (hitBreakpoints?.length === 1 && hitBreakpoints[0] === breakpointId) {
      InspectorTest.log('Hit breakpoint before calling into `this.foo`');
    } else {
      InspectorTest.log('Missed breakpoint before calling into `this.foo`');
    }
    await Promise.all([
      Protocol.Debugger.removeBreakpoint({breakpointId}),
      Protocol.Debugger.disable(),
      evalPromise,
      Protocol.Runtime.disable(),
    ]);
  },

  async function testYieldInMinifiedGenerator() {
    await Promise.all([
      Protocol.Debugger.enable(),
      Protocol.Runtime.enable(),
    ]);
    InspectorTest.log(
        'Setting breakpoint on `yield this.foo()` in `obj.minifiedGenerator`');
    const {result: {breakpointId}} =
        await Protocol.Debugger.setBreakpointByUrl({
          url,
          lineNumber: 22,
          columnNumber: 23,
        });
    InspectorTest.log('Calling `obj.minifiedGenerator().next()`');
    const pausedPromise = Protocol.Debugger.oncePaused();
    const evalPromise = Protocol.Runtime.evaluate({
      expression: 'obj.minifiedGenerator().next()',
    });
    const {params: {hitBreakpoints}} = await pausedPromise;
    if (hitBreakpoints?.length === 1 && hitBreakpoints[0] === breakpointId) {
      InspectorTest.log('Hit breakpoint before calling into `this.foo`');
    } else {
      InspectorTest.log('Missed breakpoint before calling into `this.foo`');
    }
    await Promise.all([
      Protocol.Debugger.removeBreakpoint({breakpointId}),
      Protocol.Debugger.disable(),
      evalPromise,
      Protocol.Runtime.disable(),
    ]);
  },
]);
