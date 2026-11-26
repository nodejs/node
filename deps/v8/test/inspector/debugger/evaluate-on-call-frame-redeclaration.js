// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

const {contextGroup, Protocol} = InspectorTest.start(
    'Tests var and function declarations in Debugger.evaluateOnCallFrame.');

function logResult(label, response) {
  if (response.result.exceptionDetails) {
    const desc = response.result.exceptionDetails.exception.description;
    InspectorTest.log(`${label}: ${desc.split('\n')[0]}`);
  } else {
    InspectorTest.log(
        `${label}: ok (${JSON.stringify(response.result.result.value)})`);
  }
}

async function evalOnTopFrame(callFrameId, expression) {
  const response = await Protocol.Debugger.evaluateOnCallFrame({
    callFrameId,
    expression,
  });
  logResult(expression, response);
}

InspectorTest.runAsyncTestSuite([
  async function testModuleExportsRepro() {
    await Protocol.Debugger.enable();
    contextGroup.addModule(
        `
      export let e1 = 1;
      export let e2 = 2;
      export let e3 = 3;
      export const c = 4;
      export function fn() { return 'orig'; }
      export var v = 10;
      let localLet = 20;
      const localConst = 30;
      var localVar = 40;
      function keepCaptured() { return [localLet, localConst, localVar]; }
      function f() { debugger; }
      f();
      globalThis.moduleExportsAfter = {
        e1, e2, e3, c, fn: fn(), v, localLet, localConst, localVar,
      };
    `,
        'mod-exports');

    const {params: {callFrames: [{callFrameId}]}} =
        await Protocol.Debugger.oncePaused();

    await evalOnTopFrame(callFrameId, 'function e1(){}');
    await evalOnTopFrame(callFrameId, 'function e2(){}');
    await evalOnTopFrame(callFrameId, 'function e3(){}');
    await evalOnTopFrame(callFrameId, 'var e1 = 99;');
    await evalOnTopFrame(callFrameId, '{ function e1(){} }');
    await evalOnTopFrame(callFrameId, 'function c(){}');
    await evalOnTopFrame(callFrameId, 'var c = 99;');
    await evalOnTopFrame(callFrameId, 'function fn(){}');
    await evalOnTopFrame(callFrameId, 'function localLet(){}');
    await evalOnTopFrame(callFrameId, 'var localLet = 99;');
    await evalOnTopFrame(callFrameId, 'function localConst(){}');
    await evalOnTopFrame(callFrameId, 'var missing = 1;');
    await evalOnTopFrame(callFrameId, 'function missing(){}');
    await evalOnTopFrame(callFrameId, 'var v = 11;');
    await evalOnTopFrame(callFrameId, 'function v(){ return 42; }');
    await evalOnTopFrame(callFrameId, 'var localVar = 41;');
    await evalOnTopFrame(callFrameId, 'function localVar(){ return 84; }');

    await Protocol.Debugger.resume();
    const {result: {result: {value}}} = await Protocol.Runtime.evaluate({
      expression: 'JSON.stringify(globalThis.moduleExportsAfter)',
    });
    InspectorTest.log('after: ' + value);
  },

  async function testModuleTopLevel() {
    contextGroup.addModule(
        `
      export let e = 1;
      debugger;
    `,
        'mod-toplevel');

    const {params: {callFrames: [{callFrameId}]}} =
        await Protocol.Debugger.oncePaused();
    await evalOnTopFrame(callFrameId, 'function e(){}');
    await evalOnTopFrame(callFrameId, 'var e;');
    await Protocol.Debugger.resume();
  },

  async function testModuleImports() {
    contextGroup.addModule(
        `
      export let dep1 = 1;
      export let dep2 = 2;
      export var depVar = 3;
    `,
        'mod-dep');
    contextGroup.addModule(
        `
      import {dep1, dep2, depVar} from 'mod-dep';
      import * as ns from 'mod-dep';
      function keepNs() { return ns.dep1; }
      function f() { debugger; }
      f();
      globalThis.moduleImportsAfter = {dep1, dep2, depVar, nsDep1: keepNs()};
    `,
        'mod-imports');

    const {params: {callFrames: [{callFrameId}]}} =
        await Protocol.Debugger.oncePaused();
    await evalOnTopFrame(callFrameId, 'function dep1(){}');
    await evalOnTopFrame(callFrameId, 'function dep2(){}');
    await evalOnTopFrame(callFrameId, 'function depVar(){}');
    await evalOnTopFrame(callFrameId, 'var dep1;');
    await evalOnTopFrame(callFrameId, 'function ns(){}');
    await Protocol.Debugger.resume();
    const {result: {result: {value}}} = await Protocol.Runtime.evaluate({
      expression: 'JSON.stringify(globalThis.moduleImportsAfter)',
    });
    InspectorTest.log('after: ' + value);
  },

  async function testModuleNestedEvalContexts() {
    contextGroup.addModule(
        `
      export let e = 1;
      function f() { debugger; }
      f();
    `,
        'mod-nested');

    const {params: {callFrames: [{callFrameId}]}} =
        await Protocol.Debugger.oncePaused();
    await evalOnTopFrame(callFrameId, 'with ({}) { eval("var x = 1"); }');
    await evalOnTopFrame(callFrameId, 'with ({}) { eval("function e(){}"); }');
    await evalOnTopFrame(
        callFrameId, 'try { throw 1; } catch (_) { eval("var x = 1"); }');
    await evalOnTopFrame(
        callFrameId, '{ let z = 1; (() => z); eval("var x = 1"); }');
    await evalOnTopFrame(
        callFrameId, 'let z = 1; (() => z); eval("var x = 1");');
    await evalOnTopFrame(
        callFrameId, 'let z = 1; (() => z); var x = 1;');
    await evalOnTopFrame(
        callFrameId, 'let z = 1; (() => z); function x(){}');
    await Protocol.Debugger.resume();
  },

  async function testFunctionContextBindings() {
    contextGroup.addScript(`
      function runOuterBindings() {
        const c = 1;
        let l = 2;
        var v = 3;
        function keep() { return [c, l, v]; }
        function f() { debugger; }
        f();
        return {c, l, vType: typeof v, vVal: v()};
      }
      function runOuterTdz() {
        (function f() { debugger; })();
        let l = 123;
        const c = 456;
        function keep() { return [l, c]; }
        return keep();
      }
      function runNonSimpleParams(a = (function f() { debugger; })()) {
        const keep = () => [a, arguments.length];
        return keep();
      }
    `);

    {
      const paused = Protocol.Debugger.oncePaused();
      const run = Protocol.Runtime.evaluate(
          {expression: 'JSON.stringify(runOuterBindings())'});
      const {params: {callFrames: [{callFrameId}]}} = await paused;
      await evalOnTopFrame(callFrameId, 'function c(){}');
      await evalOnTopFrame(callFrameId, 'var c = 9;');
      await evalOnTopFrame(callFrameId, 'function l(){}');
      await evalOnTopFrame(callFrameId, 'var l = 9;');
      await evalOnTopFrame(callFrameId, 'function v(){ return 77; }');
      await evalOnTopFrame(callFrameId, 'var missing = 1;');
      await Protocol.Debugger.resume();
      const {result: {result: {value}}} = await run;
      InspectorTest.log('runOuterBindings: ' + value);
    }

    {
      const paused = Protocol.Debugger.oncePaused();
      const run = Protocol.Runtime.evaluate(
          {expression: 'JSON.stringify(runOuterTdz())'});
      const {params: {callFrames: [{callFrameId}]}} = await paused;
      await evalOnTopFrame(callFrameId, 'function l(){}');
      await evalOnTopFrame(callFrameId, 'function c(){}');
      await Protocol.Debugger.resume();
      const {result: {result: {value}}} = await run;
      InspectorTest.log('runOuterTdz: ' + value);
    }

    {
      const paused = Protocol.Debugger.oncePaused();
      const run = Protocol.Runtime.evaluate(
          {expression: 'JSON.stringify(runNonSimpleParams())'});
      const {params: {callFrames: [{callFrameId}]}} = await paused;
      await evalOnTopFrame(callFrameId, 'function a(){}');
      await evalOnTopFrame(callFrameId, 'function arguments(){}');
      await Protocol.Debugger.resume();
      const {result: {result: {value}}} = await run;
      InspectorTest.log('runNonSimpleParams: ' + value);
    }
  },

  async function testStrictEvalAndStaticBlock() {
    {
      const paused = Protocol.Debugger.oncePaused();
      const run = Protocol.Runtime.evaluate({
        expression: `eval('"use strict"; const c = 1; let l = 2; (() => [c, l]); (function f() { debugger; })(); [c, l];').toString()`,
      });
      const {params: {callFrames: [{callFrameId}]}} = await paused;
      await evalOnTopFrame(callFrameId, 'function c(){}');
      await evalOnTopFrame(callFrameId, 'function l(){}');
      await evalOnTopFrame(callFrameId, 'var missing = 1;');
      await Protocol.Debugger.resume();
      const {result: {result: {value}}} = await run;
      InspectorTest.log('strictEval: ' + value);
    }

    {
      const paused = Protocol.Debugger.oncePaused();
      const run = Protocol.Runtime.evaluate({
        expression: `(() => { let res; class C { static { const c = 1; let l = 2; (() => [c, l]); (function f() { debugger; })(); res = [c, l]; } } return JSON.stringify(res); })()`,
      });
      const {params: {callFrames: [{callFrameId}]}} = await paused;
      await evalOnTopFrame(callFrameId, 'function c(){}');
      await evalOnTopFrame(callFrameId, 'function l(){}');
      await evalOnTopFrame(callFrameId, 'var missing = 1;');
      await Protocol.Debugger.resume();
      const {result: {result: {value}}} = await run;
      InspectorTest.log('staticBlock: ' + value);
    }
  },
]);
