// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

let {session, contextGroup, Protocol} = InspectorTest.start(
    'Tests that the scope chain includes outer scopes without a context.');

contextGroup.addScript(`
var shadowed = 'global';
function contextless(param) {
  let shadowed = 'outer';
  {
    let blockVar = 1;
    (function inner() { debugger; })();
  }
}

function withContext() {
  let captured = 'captured';
  {
    let blockVar = 1;
    (function inner() { captured; debugger; })();
  }
}

function emptyOuter() {
  return function inner() { debugger; };
}

function* emptyGenerator() { yield 1; }

var ctor = new Function('a', 'return function innerCtor() { debugger; }');

function directEvalOuter(param) {
  'use strict';
  let outerVar = 'outer';
  (() => {
    {
      let blockVar = 'block';
      eval('let evalVar = 1; (function innerEval() { debugger; })();');
    }
  })();
}

function chainedDirectEval() {
  let a = 1;
  {
    let b = 2;
    eval("let c = 3; { let d = 4; eval('let e = 5; (function innerMost() { debugger; })();'); }");
  }
}

function pauseInEvalCode() {
  let outerLet = 'outerLet';
  {
    let blockLet = 'blockLet';
    eval("let evalLet = 'evalLet'; debugger;");
  }
}

function sloppyDirectEval(param) {
  var fnVar = 'fnVar';
  try { throw 'err'; } catch (caught) {
    with ({withProp: 'withProp'}) {
      eval("var leaked = 'leaked'; (function sloppyInner() { debugger; })();");
    }
  }
}

function setVariableInCaller() {
  let outerVar = 'outer';
  {
    let blockVar = 'block';
    eval('(function setInner() { debugger; })();');
    return outerVar + ' ' + blockVar;
  }
}
//# sourceURL=test.js`);

// Scripts other than test.js are labelled in the order in which they appear
// in a scope chain, starting with the innermost.
const scriptUrls = new Map();
Protocol.Debugger.onScriptParsed(
    ({params: {scriptId, url}}) => scriptUrls.set(scriptId, url));
let scriptLabels;

function scriptLabel(scriptId) {
  const url = scriptUrls.get(scriptId);
  if (url === 'test.js') return '';
  if (!scriptLabels.has(scriptId)) {
    scriptLabels.set(
        scriptId, url ? `[${url}]` : `[anonymous #${scriptLabels.size + 1}]`);
  }
  return ` ${scriptLabels.get(scriptId)}`;
}

function locationToString(location) {
  return `${location.lineNumber}:${location.columnNumber}`;
}

function describe(remoteObject) {
  return remoteObject.type === 'string' ? JSON.stringify(remoteObject.value) :
                                          remoteObject.description;
}

async function logScope(scope, index) {
  let line = `[${index}] ${scope.type}`;
  if (scope.name) line += ` name=${scope.name}`;
  if (scope.emptyReason) line += ` emptyReason=${scope.emptyReason}`;
  if (scope.startLocation) {
    line += ` ${locationToString(scope.startLocation)}-` +
        `${locationToString(scope.endLocation)}` +
        scriptLabel(scope.startLocation.scriptId);
  }
  InspectorTest.log(line);
  if (scope.type === 'global') return;
  const {result: {result: variables}} = await Protocol.Runtime.getProperties(
      {objectId: scope.object.objectId, ownProperties: true});
  for (const variable of variables) {
    const value =
        'value' in variable ? describe(variable.value) : '<value_unavailable>';
    InspectorTest.log(`    ${variable.name} : ${value}`);
  }
}

async function logScopeChain(callFrame) {
  scriptLabels = new Map();
  for (let i = 0; i < callFrame.scopeChain.length; ++i) {
    await logScope(callFrame.scopeChain[i], i);
  }
}

async function evaluateInEachScope(callFrame, expressions) {
  for (let i = 0; i < callFrame.scopeChain.length; ++i) {
    for (const expression of expressions) {
      const {result: {result, exceptionDetails}} =
          await Protocol.Debugger.evaluateOnCallFrame(
              {callFrameId: callFrame.callFrameId, expression, scopeNumber: i});
      const value = exceptionDetails ?
          exceptionDetails.exception.description.split('\n')[0] :
          describe(result);
      InspectorTest.log(`  scopeNumber ${i}: ${expression} = ${value}`);
    }
  }
}

async function pauseAndLogScopes(
    expression, evaluateExpressions = [], whilePaused = async () => {}) {
  const paused = Protocol.Debugger.oncePaused();
  const evaluated = Protocol.Runtime.evaluate({expression});
  const {params: {callFrames: [callFrame]}} = await paused;
  InspectorTest.log(`Paused in ${callFrame.functionName}:`);
  await logScopeChain(callFrame);
  await evaluateInEachScope(callFrame, evaluateExpressions);
  await whilePaused(callFrame);
  await Protocol.Debugger.resume();
  return (await evaluated).result.result;
}

InspectorTest.runAsyncTestSuite([
  async function setUp() {
    await Protocol.Debugger.enable();
    await Protocol.Runtime.enable();
  },

  async function testContextlessOuterScopes() {
    await pauseAndLogScopes(
        'contextless(42)', ['shadowed', 'blockVar', 'param']);
  },

  async function testContextfulOuterScopes() {
    await pauseAndLogScopes('withContext()', ['captured', 'blockVar']);
  },

  async function testEmptyOuterFunction() {
    await pauseAndLogScopes('emptyOuter()()', ['shadowed']);
  },

  async function testFunctionConstructor() {
    await pauseAndLogScopes('ctor(42)()', ['a']);
  },

  async function testWrappedFunction() {
    const paused = Protocol.Debugger.oncePaused();
    utils.compileAndRunWrapped(
        contextGroup.id,
        'let wrappedVar = 1; ' +
            '{ let blockVar = 2; (function inner() { debugger; })(); }',
        'wrapped.js');
    const {params: {callFrames: [callFrame]}} = await paused;
    await logScopeChain(callFrame);
    await Protocol.Debugger.resume();
  },

  async function testGeneratorScopesKeepEmptyLocalScope() {
    const {result: {result: {objectId}}} =
        await Protocol.Runtime.evaluate({expression: 'emptyGenerator()'});
    const {result: {internalProperties}} =
        await Protocol.Runtime.getProperties({objectId});
    const scopes = internalProperties.find(p => p.name === '[[Scopes]]');
    const {result: {result: entries}} = await Protocol.Runtime.getProperties(
        {objectId: scopes.value.objectId, ownProperties: true});
    for (const entry of entries) {
      if (entry.value) {
        InspectorTest.log(`${entry.name}: ${entry.value.description}`);
      }
    }
  },

  async function testDirectEvalOuterScopes() {
    await pauseAndLogScopes(
        'directEvalOuter(42)', ['outerVar', 'evalVar', 'param']);
  },

  async function testChainedDirectEvalOuterScopes() {
    await pauseAndLogScopes(
        'chainedDirectEval()', ['a', 'b', 'c', 'd', 'e']);
  },

  async function testPauseInDirectEvalCode() {
    await pauseAndLogScopes(
        'pauseInEvalCode()', ['outerLet', 'blockLet', 'evalLet']);
  },

  async function testSloppyDirectEvalInCatchAndWith() {
    await pauseAndLogScopes(
        'sloppyDirectEval(42)',
        ['fnVar', 'caught', 'withProp', 'leaked', 'param']);
  },

  async function testSetVariableValueInDirectEvalCaller() {
    const result = await pauseAndLogScopes(
        'setVariableInCaller()', [], async (callFrame) => {
          for (const [scopeNumber, variableName] of [[1, 'blockVar'],
                                                     [2, 'outerVar']]) {
            const {error} = await Protocol.Debugger.setVariableValue({
              callFrameId: callFrame.callFrameId,
              scopeNumber,
              variableName,
              newValue: {value: `new ${variableName}`},
            });
            InspectorTest.log(`  setVariableValue(${scopeNumber}, ${
                variableName}): ${error ? error.message : 'ok'}`);
          }
        });
    InspectorTest.log(`Result: ${describe(result)}`);
  },
]);
