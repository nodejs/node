// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

let {session, contextGroup, Protocol} = InspectorTest.start(
    'Tests that [[Scopes]] of functions and generators only list non-empty ' +
    'scopes of the runtime context chain.');

contextGroup.addScript(`
var g = 'global';
function outerCtx(kind) {
  let captured = 1; let notCaptured = 2;
  { let blk = 3;
    return kind ? function* gen() { captured; yield 1; }
                : function fn() { captured; }; }
}
function outerNoCtx(kind, p) {
  let x = 1;
  { let b = 2;
    return kind ? function* gen() { yield 1; } : function fn() {}; }
}
function emptyOuter(kind) {
  return kind ? function* gen() {} : function fn() {};
}
function catchOuter(kind) {
  try { throw 1; } catch (e) {
    return kind ? function* gen() { e; } : function fn() { e; };
  }
}
function catchOuterNoCtx(kind) {
  try { throw 1; } catch (e) {
    let q = e;
    return kind ? function* gen() {} : function fn() {};
  }
}
function evalOuter(kind) {
  eval('var ev = 1');
  return kind ? function* gen() { ev; } : function fn() { ev; };
}
function classOuter(kind) {
  class C {
    static m() { return kind ? function* gen() { C; } : function fn() { C; }; }
  }
  return C.m();
}
function classOuterNoRef(kind) {
  class C { static m() { return kind ? function* gen() {} : function fn() {}; } }
  return C.m();
}
function withOuter(kind) {
  with ({w: 1}) { return kind ? function* gen() { w; } : function fn() { w; }; }
}
function innerBlocks() {
  return function* gen() { let a = 1; { let b = 2; yield b; } };
}
var innerGen = innerBlocks()(); innerGen.next();
//# sourceURL=test.js`);

contextGroup.addModule(`
export let m = 1;
globalThis.modGen = function* gen() { m; };
globalThis.modFn = function fn() { m; };
`, 'module.js');
contextGroup.addModule(`
globalThis.modGenEmpty = function* gen() {};
globalThis.modFnEmpty = function fn() {};
`, 'empty-module.js');

function describe(value) {
  if (value.type === 'function') return 'function';
  return value.value ?? value.description;
}

async function logScopes(expression) {
  const {result: {result: {objectId}}} =
      await Protocol.Runtime.evaluate({expression});
  const {result: {internalProperties}} =
      await Protocol.Runtime.getProperties({objectId});
  const scopes = internalProperties.find(p => p.name === '[[Scopes]]');
  const {result: {result: entries}} = await Protocol.Runtime.getProperties(
      {objectId: scopes.value.objectId, ownProperties: true});
  InspectorTest.log(expression);
  for (const entry of entries) {
    if (!entry.value || entry.name === '__proto__') continue;
    let line = `  ${entry.name}: ${entry.value.description}`;
    if (!entry.value.description.startsWith('Global')) {
      const {result: {result: variables}} = await Protocol.Runtime.getProperties(
          {objectId: entry.value.objectId, ownProperties: true});
      line += ' {' +
          variables
              .map(v => `${v.name}=${
                       'value' in v ? describe(v.value) : '<unavailable>'}`)
              .join(', ') +
          '}';
    }
    InspectorTest.log(line);
  }
}

(async function test() {
  await Protocol.Runtime.enable();
  for (const f of ['outerCtx', 'outerNoCtx', 'emptyOuter', 'catchOuter',
                   'catchOuterNoCtx', 'evalOuter', 'classOuter',
                   'classOuterNoRef', 'withOuter']) {
    await logScopes(`${f}(false)`);
    await logScopes(`${f}(true)()`);
  }
  await logScopes('innerGen');
  await logScopes('modFn');
  await logScopes('modGen()');
  await logScopes('modFnEmpty');
  await logScopes('modGenEmpty()');
  InspectorTest.completeTest();
})();
