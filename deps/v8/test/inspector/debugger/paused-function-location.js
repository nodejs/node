// Copyright 2025 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

const { session, contextGroup, Protocol } = InspectorTest.start('Change detector for \'functionLocation\' for various callables');

const snippets = [
  `
function functionDeclaration() {
  debugger;
}
functionDeclaration();
`, `
async function asyncFnDeclaration() {
  debugger;
}
asyncFnDeclaration();
`, `
function* generator() {
  debugger;
  yield;
}
generator().next();
`, `
async function* asyncGenerator() {
  debugger;
  yield;
}
asyncGenerator().next();
`, `
const anonFnExpr = function () {
  debugger;
};
anonFnExpr();
`, `
const namedFnExpr = function foo() {
  debugger;
}
namedFnExpr();
`, `
const asyncAnonFnExpr = async function() {
  debugger;
}
asyncAnonFnExpr();
`, `
const anonGeneratorExpr = function* () {
  debugger;
}
anonGeneratorExpr().next();
`, `
const arrowFn = () => {
  debugger;
}
arrowFn();
`, `
const asyncArrowFn = async () => {
  debugger;
}
asyncArrowFn();
`, `
const pause = () => { debugger; }
const implicitReturnArrowFn = x => pause();
implicitReturnArrowFn(42);
`, `
const pause2 = () => { debugger; }
const implicitReturnAsyncArrowFn = async x => await pause2();
implicitReturnAsyncArrowFn(42);
`, `
const obj = {
  method() {
    debugger;
  }
}
obj.method();
`, `
const obj2 = {
  async method() {
    debugger;
  }
}
obj2.method();
`, `
const obj3 = {
  * generator() {
    debugger;
    yield;
  }
}
obj3.generator().next();
`, `
const obj4 = {
  async * generator() {
    debugger;
    yield;
  }
}
obj4.generator().next();
`, `
const obj5 = {
  ['computedProp']() {
    debugger;
  }
}
obj5.computedProp();
`, `
const obj6 = {
  get getAccessor() {
    debugger;
  }
}
obj6.getAccessor;
`, `
const obj7 = {
  set setAccessor(value) {
    debugger;
  }
}
obj7.setAccessor = 42;
`, `
class C1 {
  constructor() {
    debugger;
  }
}
new C1();
`, `
const pause3 = () => { debugger; }
class C2 {
  #field = pause3();
}
new C2();
`, `
class C3 {
  static staticMethod() {
    debugger;
  }
}
C3.staticMethod();
`, `
class C4 {
  #privateMethod() {
    debugger;
  }

  invoke() {
    this.#privateMethod();
  }
}
new C4().invoke();
`, `
class C5 {
  static {
    debugger;
  }
}
`,
];

function logSnippetWithLocation(snippet, location) {
  if (!location) {
    return;
  }

  const lines = snippet.split('\n');
  const { lineNumber, columnNumber } = location;
  const marker = ' '.repeat(columnNumber) + '^';

  InspectorTest.log([
    lines[lineNumber - 1],
    lines[lineNumber],
    marker,
    lines[lineNumber + 1],
    lines[lineNumber + 2],
  ].filter(line => !!line).join('\n'));
  InspectorTest.log('\n');
}

(async () => {
  await Protocol.Debugger.enable();

  for (const snippet of snippets) {
    const expression = snippet.trim();
    const evaluatePromise = Protocol.Runtime.evaluate({ expression, awaitPromise: true });
    const { params: { callFrames } } = await Protocol.Debugger.oncePaused();

    InspectorTest.log('---------------------------------------------');
    for (const frame of callFrames.slice(0, -1)) { // Drop the top frame (it's the CDP eval)
      logSnippetWithLocation(expression, frame.functionLocation);
    }

    await Promise.all([evaluatePromise, Protocol.Debugger.resume()]);
  }

  InspectorTest.completeTest();
})();
