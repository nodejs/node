// Copyright 2022 the V8 project authors. All rights reserved.
// Use  of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

let {session, contextGroup, Protocol} =
    InspectorTest.start('Line breakpoints for await');

// clang-format off
const url = 'line-breakpoint-await.js';
contextGroup.addScript(`
var obj = {
  foo() {
    debugger;
  },

  async bar() {
    await this.foo();
  },

  async baz() {
await this.foo();
  },

  async* barGenerator() {
    await this.foo();
  },

  async* bazGenerator() {
await this.foo();
  },

  async minified(){await this.foo();}
};
`, 0, 0, url);
// clang-format on

session.setupScriptMap();
InspectorTest.runAsyncTestSuite([
  async function testAwaitInAsyncFunctionWithLeadingWhitespace() {
    await Promise.all([
      Protocol.Debugger.enable(),
      Protocol.Runtime.enable(),
    ]);
    InspectorTest.log('Setting breakpoint on `await this.foo()` in `obj.bar`');
    const {result: {breakpointId}} =
        await Protocol.Debugger.setBreakpointByUrl({
          url,
          lineNumber: 7,
          columnNumber: 0,
        });
    InspectorTest.log('Calling `obj.bar()`');
    const pausedPromise = Protocol.Debugger.oncePaused();
    const evalPromise = Protocol.Runtime.evaluate({
      expression: 'obj.bar()',
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

  async function testAwaitInAsyncFunctionWithoutLeadingWhitespace() {
    await Promise.all([
      Protocol.Debugger.enable(),
      Protocol.Runtime.enable(),
    ]);
    InspectorTest.log('Setting breakpoint on `await this.foo()` in `obj.baz`');
    const {result: {breakpointId}} =
        await Protocol.Debugger.setBreakpointByUrl({
          url,
          lineNumber: 11,
          columnNumber: 0,
        });
    InspectorTest.log('Calling `obj.baz()`');
    const pausedPromise = Protocol.Debugger.oncePaused();
    const evalPromise = Protocol.Runtime.evaluate({
      expression: 'obj.baz()',
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

  async function testAwaitInAsyncGeneratorWithLeadingWhitespace() {
    await Promise.all([
      Protocol.Debugger.enable(),
      Protocol.Runtime.enable(),
    ]);
    InspectorTest.log(
        'Setting breakpoint on `await this.foo()` in `obj.barGenerator`');
    const {result: {breakpointId}} =
        await Protocol.Debugger.setBreakpointByUrl({
          url,
          lineNumber: 15,
          columnNumber: 0,
        });
    InspectorTest.log('Calling `obj.barGenerator().next()`');
    const pausedPromise = Protocol.Debugger.oncePaused();
    const evalPromise = Protocol.Runtime.evaluate({
      expression: 'obj.barGenerator().next()',
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

  async function testAwaitInAsyncGeneratorWithoutLeadingWhitespace() {
    await Promise.all([
      Protocol.Debugger.enable(),
      Protocol.Runtime.enable(),
    ]);
    InspectorTest.log(
        'Setting breakpoint on `await this.foo()` in `obj.bazGenerator`');
    const {result: {breakpointId}} =
        await Protocol.Debugger.setBreakpointByUrl({
          url,
          lineNumber: 19,
          columnNumber: 0,
        });
    InspectorTest.log('Calling `obj.bazGenerator().next()`');
    const pausedPromise = Protocol.Debugger.oncePaused();
    const evalPromise = Protocol.Runtime.evaluate({
      expression: 'obj.bazGenerator().next()',
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

  async function testAwaitInAsyncFunctionMinified() {
    await Promise.all([
      Protocol.Debugger.enable(),
      Protocol.Runtime.enable(),
    ]);
    InspectorTest.log(
        'Setting breakpoint on `await this.foo()` in `obj.minified`');
    const {result: {breakpointId}} =
        await Protocol.Debugger.setBreakpointByUrl({
          url,
          lineNumber: 22,
          columnNumber: 19,
        });
    InspectorTest.log('Calling `obj.minified()`');
    const pausedPromise = Protocol.Debugger.oncePaused();
    const evalPromise = Protocol.Runtime.evaluate({
      expression: 'obj.minified()',
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
]);
