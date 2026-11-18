// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

let {session, contextGroup, Protocol} = InspectorTest.start(
    'Tests scopes of a direct eval whose not-yet-pushed block scope starts ' +
    'at the same offset as a block scope in the caller script.');

// The block in `outer` and the `for await` block inside the eval string start
// at the same character offset (in their respective scripts). The generator is
// suspended in the `for await` header, before its block context is pushed, so
// the generator's context is still the caller's block context.
const kEvalPrefix = '(async function*(){';
const kForHeader = 'for await (';
let source = `
var never = {[Symbol.asyncIterator]() {
  return {next() { return new Promise(() => {}); }};
}};
function outer() {
                                              { /* BLOCK_START */
    let a = 111;
    () => a;
    let eval_str = "${kEvalPrefix}" + " ".repeat(PAD) +
        "${kForHeader}const [x1,x2,x3,x4,x5,x6] of never) { () => [x1,x2,x3,x4,x5,x6]; }})()";
    return eval(eval_str);
  }
}
var g = outer();
g.next();
`;
const blockOffset = source.indexOf('{ /* BLOCK_START */');
source = source.replace(
    'PAD', blockOffset - kEvalPrefix.length - kForHeader.length);
contextGroup.addScript(source);

InspectorTest.runAsyncTestSuite([
  async function testGeneratorScopes() {
    const {result: {result: {objectId}}} =
        await Protocol.Runtime.evaluate({expression: 'g'});
    const {result: {internalProperties}} =
        await Protocol.Runtime.getProperties({objectId});
    const state = internalProperties.find(p => p.name === '[[GeneratorState]]');
    InspectorTest.log('state: ' + state.value.value);
    const scopes = internalProperties.find(p => p.name === '[[Scopes]]');
    const {result: {result: scopeList}} = await Protocol.Runtime.getProperties(
        {objectId: scopes.value.objectId, ownProperties: true});
    for (const scope of scopeList) {
      if (!scope.value || !scope.value.objectId) continue;
      InspectorTest.log(scope.value.description);
      if (scope.value.description === 'Global') continue;
      const {result: {result: vars}} = await Protocol.Runtime.getProperties(
          {objectId: scope.value.objectId, ownProperties: true});
      for (const v of vars) {
        const value = v.value ?
            (v.value.description ?? v.value.value ?? v.value.type) :
            '<unavailable>';
        InspectorTest.log(`  ${v.name}: ${value}`);
      }
    }
  },
]);
