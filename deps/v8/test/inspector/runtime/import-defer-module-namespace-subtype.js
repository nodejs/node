// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --js-defer-import-eval

const {session, contextGroup, Protocol} = InspectorTest.start(
    'Checks that a deferred module namespace reports the "deferredmodule" ' +
    'subtype, and that no other object can claim it.');

contextGroup.addModuleWithoutEvaluating(
    `
globalThis.deferredEvaluated = true;
export const constExport = 1;
`,
    'deferred');

contextGroup.addModuleWithoutEvaluating(
    `
export const constExport = 1;
`,
    'plain');

contextGroup.addModule(
    `
import defer * as deferredNs from 'deferred';
import * as plainNs from 'plain';
globalThis.deferredNs = deferredNs;
globalThis.plainNs = plainNs;
globalThis.holder = {deferredNs};
// An ordinary object dressed up as a deferred namespace: the class name and
// the internal-looking property are both forgeable, the subtype is not.
globalThis.forged = {
  [Symbol.toStringTag]: 'Deferred Module',
  '[[ModuleStatus]]': 'linked',
};
`,
    'main');

async function evaluate(expression) {
  const {result: {result}} =
      await Protocol.Runtime.evaluate({expression, generatePreview: true});
  return result;
}

function logSubtype(what, remoteObject) {
  InspectorTest.log(`${what}: type=${remoteObject.type} subtype=${
      remoteObject.subtype} className=${remoteObject.className}`);
}

async function logModuleStatus(what, objectId) {
  const {result: {internalProperties}} =
      await Protocol.Runtime.getProperties({objectId, ownProperties: true});
  const status =
      (internalProperties || []).find(p => p.name === '[[ModuleStatus]]');
  InspectorTest.log(
      `${what}: [[ModuleStatus]]=${status && status.value.value}`);
}

(async function test() {
  await Protocol.Runtime.enable();
  await InspectorTest.waitForPendingTasks();

  InspectorTest.log('\nRuntime.evaluate on the unevaluated deferred namespace:');
  const deferredNs = await evaluate('globalThis.deferredNs');
  logSubtype('deferredNs', deferredNs);
  await logModuleStatus('deferredNs', deferredNs.objectId);

  InspectorTest.log(
      '\nThe namespace as a property value of the global object:');
  const {result: {result: globalProps}} = await Protocol.Runtime.evaluate(
      {expression: 'globalThis', generatePreview: true});
  const {result: {result: properties}} = await Protocol.Runtime.getProperties(
      {objectId: globalProps.objectId, ownProperties: true});
  for (const name of ['deferredNs', 'plainNs', 'forged']) {
    logSubtype(name, properties.find(p => p.name === name).value);
  }

  InspectorTest.log('\nNested in another object\'s preview:');
  const holder = await evaluate('globalThis.holder');
  for (const property of holder.preview.properties) {
    InspectorTest.log(`${property.name}: type=${property.type} subtype=${
        property.subtype} value=${property.value}`);
  }

  InspectorTest.log('\nForcing evaluation of the deferred module:');
  InspectorTest.logMessage(await Protocol.Runtime.callFunctionOn({
    objectId: deferredNs.objectId,
    functionDeclaration: 'function() { return Object.keys(this).join(","); }',
    returnByValue: true,
  }));

  InspectorTest.log('\nThe subtype survives evaluation:');
  const evaluatedNs = await evaluate('globalThis.deferredNs');
  logSubtype('deferredNs', evaluatedNs);
  await logModuleStatus('deferredNs', evaluatedNs.objectId);

  InspectorTest.completeTest();
})();
