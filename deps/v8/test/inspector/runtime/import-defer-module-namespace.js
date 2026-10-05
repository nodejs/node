// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --js-defer-import-eval

const {session, contextGroup, Protocol} = InspectorTest.start(
    'Checks that the inspector describes a deferred module namespace without ' +
    'evaluating the module. Until the module runs, the namespace reports no ' +
    'properties at all -- reading an export, or even establishing its ' +
    'enumerability, would evaluate the module -- and [[ModuleStatus]] ' +
    'describes it instead.');

contextGroup.addModuleWithoutEvaluating(
    `
globalThis.deferredEvaluated = true;
export const constExport = 1;
export let letExport = 2;
export function fnExport() {}
`,
    'deferred');

contextGroup.addModuleWithoutEvaluating(
    `
throw new Error('module body threw');
export const neverInitialized = 1;
`,
    'throwing');

// A side-effect-only module: it has no export whose read could force the
// evaluation, so `Object.keys` is the only way to trigger it.
contextGroup.addModuleWithoutEvaluating(
    `
globalThis.sideEffectOnlyEvaluated = true;
`,
    'side-effect-only');

contextGroup.addModule(
    `
import defer * as deferredNs from 'deferred';
import defer * as throwingNs from 'throwing';
import defer * as sideEffectOnlyNs from 'side-effect-only';
globalThis.deferredNs = deferredNs;
globalThis.throwingNs = throwingNs;
globalThis.sideEffectOnlyNs = sideEffectOnlyNs;
globalThis.holder = {deferredNs};
`,
    'main');

async function evaluateToString(expression) {
  const {result: {result}} =
      await Protocol.Runtime.evaluate({expression: `String(${expression})`});
  return result.value;
}

async function logDeferredEvaluated(when) {
  InspectorTest.log(
      `globalThis.deferredEvaluated ${when}: ` +
      await evaluateToString('globalThis.deferredEvaluated'));
}

async function namespaceWithPreview(expression) {
  const {result: {result}} =
      await Protocol.Runtime.evaluate({expression, generatePreview: true});
  return result;
}

(async function test() {
  await Protocol.Runtime.enable();
  await InspectorTest.waitForPendingTasks();

  await logDeferredEvaluated('before inspecting');

  InspectorTest.log('\nRemoteObject with preview for the deferred namespace:');
  const ns = await namespaceWithPreview('globalThis.deferredNs');
  InspectorTest.logMessage(ns);
  await logDeferredEvaluated('after building the preview');

  // Every command that wraps a value with a preview must describe the
  // namespace the same way, whether it is the value of an expression, the
  // result a promise settles with, or the completion value of a script.
  InspectorTest.log('\nThe same description through Runtime.awaitPromise:');
  const {result: {result: promise}} = await Protocol.Runtime.evaluate(
      {expression: 'Promise.resolve(globalThis.deferredNs)'});
  const {result: {result: awaited}} = await Protocol.Runtime.awaitPromise(
      {promiseObjectId: promise.objectId, generatePreview: true});
  InspectorTest.logMessage(awaited);
  await logDeferredEvaluated('after Runtime.awaitPromise');

  InspectorTest.log('\nThe same description through Runtime.runScript:');
  const {result: {scriptId}} = await Protocol.Runtime.compileScript({
    expression: 'globalThis.deferredNs',
    sourceURL: 'namespace.js',
    persistScript: true,
  });
  const {result: {result: scriptResult}} =
      await Protocol.Runtime.runScript({scriptId, generatePreview: true});
  InspectorTest.logMessage(scriptResult);
  await logDeferredEvaluated('after Runtime.runScript');

  InspectorTest.log('\nRuntime.getProperties on the deferred namespace:');
  InspectorTest.logMessage(await Protocol.Runtime.getProperties(
      {objectId: ns.objectId, ownProperties: true, generatePreview: true}));
  await logDeferredEvaluated('after Runtime.getProperties');

  InspectorTest.log('\nForcing evaluation through Object.keys:');
  InspectorTest.logMessage(await Protocol.Runtime.callFunctionOn({
    objectId: ns.objectId,
    functionDeclaration: 'function() { return Object.keys(this).join(","); }',
    returnByValue: true,
  }));
  await logDeferredEvaluated('after forcing evaluation');

  InspectorTest.log('\nRemoteObject with preview once evaluated:');
  InspectorTest.logMessage(await namespaceWithPreview('globalThis.deferredNs'));

  InspectorTest.log('\nRuntime.getProperties once evaluated:');
  InspectorTest.logMessage(await Protocol.Runtime.getProperties(
      {objectId: ns.objectId, ownProperties: true, generatePreview: true}));

  InspectorTest.log('\nA deferred namespace nested in another object:');
  InspectorTest.logMessage(await namespaceWithPreview('globalThis.holder'));

  InspectorTest.log('\nA module with no exports:');
  const sideEffectOnlyNs =
      await namespaceWithPreview('globalThis.sideEffectOnlyNs');
  InspectorTest.logMessage(sideEffectOnlyNs);
  InspectorTest.log(
      'globalThis.sideEffectOnlyEvaluated after building the preview: ' +
      await evaluateToString('globalThis.sideEffectOnlyEvaluated'));

  InspectorTest.log('\nObject.keys forces evaluation with no exports to read:');
  InspectorTest.logMessage(await Protocol.Runtime.callFunctionOn({
    objectId: sideEffectOnlyNs.objectId,
    functionDeclaration: 'function() { return Object.keys(this).join(","); }',
    returnByValue: true,
  }));
  InspectorTest.log(
      'globalThis.sideEffectOnlyEvaluated after forcing evaluation: ' +
      await evaluateToString('globalThis.sideEffectOnlyEvaluated'));

  InspectorTest.log('\nA module whose body throws:');
  const throwingNs = await namespaceWithPreview('globalThis.throwingNs');
  InspectorTest.logMessage(throwingNs);
  InspectorTest.log('\nForcing evaluation of the throwing module:');
  InspectorTest.logMessage(await Protocol.Runtime.callFunctionOn({
    objectId: throwingNs.objectId,
    functionDeclaration: 'function() { return Object.keys(this).join(","); }',
    returnByValue: true,
  }));
  InspectorTest.log('\nRemoteObject with preview after the module threw:');
  InspectorTest.logMessage(
      await namespaceWithPreview('globalThis.throwingNs'));

  InspectorTest.completeTest();
})();
