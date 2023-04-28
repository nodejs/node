// Copyright 2017 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

let {session, contextGroup, Protocol} =
  InspectorTest.start('Tests evaluateOnCallFrame in module.');

var module1 = `
let a1 = 10;
let g1 = 1;
export let b1 = 11;
export function foo1() {
  let c1 = 12;
  let g1 = 2;
  debugger;
  return a1 + b1 + c1 + g1;
};
export default 42;
`;

var module2 = `
import { foo1 } from 'module1';
let a2 = 20;
export * as mod1 from 'module1';
export let b2 = 21;
export function foo2() {
  let c2 = 22;
  return foo1() + a2 + b2 + c2;
}`;

var module3 = `
import { foo2 } from 'module2';
let a3 = 30;
export let b3 = 31;
foo2();
`;

var module4 = `
let a = 1;
let b = 2;
function bar() {
  let a = 0;
  (() => {a; debugger;})();
};
bar();`;

var module5 = `
import { b2 } from 'module2';
export const a = 0;
export let b = 0;
export var c = 0;
debugger;
`;

var module6 = `
let x = 5;
(function() { let y = x; debugger; })()
`;

var module7 = `
let x = 5;
debugger;
`;

InspectorTest.runAsyncTestSuite([
  async function testTotal() {
    session.setupScriptMap();
    Protocol.Debugger.enable();
    contextGroup.addModule(module1, 'module1');
    contextGroup.addModule(module2, 'module2');
    contextGroup.addModule(module3, 'module3');
    let {params:{callFrames}} = (await Protocol.Debugger.oncePaused());
    session.logCallFrames(callFrames);
    for (let i = 0; i < callFrames.length; ++i) {
      await checkFrame(callFrames[i], i);
    }
    await Protocol.Debugger.resume();
  },

  async function testAnother() {
    contextGroup.addModule(module4, 'module4');
    let {params:{callFrames}} = (await Protocol.Debugger.oncePaused());
    session.logCallFrames(callFrames);
    for (let i = 0; i < callFrames.length; ++i) {
      await checkFrame(callFrames[i], i);
    }
    await Protocol.Debugger.resume();
  },

  async function testDifferentModuleVariables() {
    contextGroup.addModule(module5, 'module5');
    let {params:{callFrames}} = (await Protocol.Debugger.oncePaused());
    session.logCallFrames(callFrames);
    for (let i = 0; i < callFrames.length; ++i) {
      await checkFrame(callFrames[i], i);
    }
    await Protocol.Debugger.resume();
  },

  async function testCapturedLocalVariable() {
    contextGroup.addModule(module6, 'module6');
    let {params:{callFrames}} = (await Protocol.Debugger.oncePaused());
    session.logCallFrames(callFrames);
    for (let i = 0; i < callFrames.length; ++i) {
      await checkFrame(callFrames[i], i);
    }
    await Protocol.Debugger.resume();
  },

  async function testLocalVariableToplevel() {
    contextGroup.addModule(module7, 'module7');
    let {params:{callFrames}} = (await Protocol.Debugger.oncePaused());
    session.logCallFrames(callFrames);
    for (let i = 0; i < callFrames.length; ++i) {
      await checkFrame(callFrames[i], i);
    }
    await Protocol.Debugger.resume();
  }
]);

async function checkFrame(frame, i) {
  let variables = new Set();
  variables.add('Array');

  for (let scopeChain of frame.scopeChain) {
    if (scopeChain.name) {
      InspectorTest.log(scopeChain.type + ':' + scopeChain.name);
    } else {
      InspectorTest.log(scopeChain.type);
    }
    if (scopeChain.type === 'global') {
      InspectorTest.log('[\n  ...\n]');
      continue;
    }
    let {result: {result}} = await Protocol.Runtime.getProperties({
        objectId: scopeChain.object.objectId});
    result.forEach(v => variables.add(v.name));
    result = result.map(v => v.value  ?
       (v.name + ' = ' + v.value.description) : (v.name));
    InspectorTest.logMessage(result);
  }

  InspectorTest.log(`Check variables in frame#${i}`);
  await session.logSourceLocation(frame.location);
  await checkVariables(frame.callFrameId, variables);
}

async function checkVariables(callFrameId, names) {
  for (let name of names) {
    var {result:{result}} = await Protocol.Debugger.evaluateOnCallFrame({
      callFrameId, expression: name});
    if (result.type === 'object' && result.subtype && result.subtype === 'error') {
      continue;
    }
    InspectorTest.log(name + ' = ');
    InspectorTest.logMessage(result);
    if (result.type === "number") {
      let updateExpression = '++' + name;
      InspectorTest.log('Evaluating: ' + updateExpression);
      await Protocol.Debugger.evaluateOnCallFrame({
        callFrameId, expression: updateExpression});

      var {result:{result}} = await Protocol.Debugger.evaluateOnCallFrame({
        callFrameId, expression: name});
      InspectorTest.log('updated ' + name + ' = ');
      InspectorTest.logMessage(result);

      updateExpression = '--' + name;
      InspectorTest.log('Evaluating: ' + updateExpression);
      await Protocol.Debugger.evaluateOnCallFrame({
        callFrameId, expression: updateExpression});
    }
  }
}
