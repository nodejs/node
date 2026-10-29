// Copyright 2016 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

let {session, contextGroup, Protocol} = InspectorTest.start("Check that while Runtime.getProperties call on proxy object no user defined trap will be executed.");

contextGroup.addScript(`
var self = this;
function testFunction(revocable)
{
  self.counter = 0;
  var handler = {
    get: function(target, name){
      self.counter++;
      return Reflect.get.apply(this, arguments);
    },
    set: function(target, name){
      self.counter++;
      return Reflect.set.apply(this, arguments);
    },
    getPrototypeOf: function(target) {
      self.counter++;
      return Reflect.getPrototypeOf.apply(this, arguments);
    },
    setPrototypeOf: function(target) {
      self.counter++;
      return Reflect.setPrototypeOf.apply(this, arguments);
    },
    isExtensible: function(target) {
      self.counter++;
      return Reflect.isExtensible.apply(this, arguments);
    },
    isExtensible: function(target) {
      self.counter++;
      return Reflect.isExtensible.apply(this, arguments);
    },
    isExtensible: function(target) {
      self.counter++;
      return Reflect.isExtensible.apply(this, arguments);
    },
    preventExtensions: function() {
      self.counter++;
      return Reflect.preventExtensions.apply(this, arguments);
    },
    getOwnPropertyDescriptor: function() {
      self.counter++;
      return Reflect.getOwnPropertyDescriptor.apply(this, arguments);
    },
    defineProperty: function() {
      self.counter++;
      return Reflect.defineProperty.apply(this, arguments);
    },
    has: function() {
      self.counter++;
      return Reflect.has.apply(this, arguments);
    },
    get: function() {
      self.counter++;
      return Reflect.get.apply(this, arguments);
    },
    set: function() {
      self.counter++;
      return Reflect.set.apply(this, arguments);
    },
    deleteProperty: function() {
      self.counter++;
      return Reflect.deleteProperty.apply(this, arguments);
    },
    ownKeys: function() {
      self.counter++;
      return Reflect.ownKeys.apply(this, arguments);
    },
    apply: function() {
      self.counter++;
      return Reflect.apply.apply(this, arguments);
    },
    construct: function() {
      self.counter++;
      return Reflect.construct.apply(this, arguments);
    }
  };
  var obj = { a : 1 };
  if (revocable) {
    var revocableProxy = Proxy.revocable(obj, handler);
    return [revocableProxy.proxy, revocableProxy.revoke]
  } else {
    return new Proxy(obj, handler);
  }
}`);

function getArrayElement(arrayObjectId, idx) {
  return Protocol.Runtime.callFunctionOn({
    functionDeclaration: `function() { return this[${idx}]; }`,
    objectId: arrayObjectId
  });
}

async function testRegular() {
  InspectorTest.logMessage("Testing regular Proxy");

  var result = await Protocol.Runtime.evaluate({ expression: "testFunction(false)", generatePreview: true });
  InspectorTest.logMessage(result);
  var proxyId = result.result.result.objectId;
  InspectorTest.logMessage(await Protocol.Runtime.getProperties({ objectId: proxyId, generatePreview: true }));
}

async function testRevocable() {
  InspectorTest.logMessage("Testing revocable Proxy");

  var result = await Protocol.Runtime.evaluate({ expression: "testFunction(true)" });
  var proxyInfo = await getArrayElement(result.result.result.objectId, 0);
  var revokeInfo = await getArrayElement(result.result.result.objectId, 1);
  var proxyId = proxyInfo.result.result.objectId;
  InspectorTest.logMessage(await Protocol.Runtime.callFunctionOn({
    functionDeclaration: `function() { return this; }`,
    objectId: proxyId,
    generatePreview: true
  }))
  InspectorTest.logMessage(await Protocol.Runtime.getProperties({ objectId: proxyId, generatePreview: true }));
  await Protocol.Runtime.callFunctionOn({
    functionDeclaration: `function() { this(); }`,
    objectId: revokeInfo.result.result.objectId
  });
  InspectorTest.logMessage(await Protocol.Runtime.callFunctionOn({
    functionDeclaration: `function() { return this; }`,
    objectId: proxyId,
    generatePreview: true
  }))
  InspectorTest.logMessage(await Protocol.Runtime.getProperties({ objectId: proxyId, generatePreview: true }));
}

async function checkCounter() {
  InspectorTest.logMessage("Checking counter");

  var result = await Protocol.Runtime.evaluate({ expression: "self.counter" });
  InspectorTest.logMessage(result);
}

(async function test() {
  await testRegular();
  await testRevocable();
  await checkCounter();
  InspectorTest.completeTest();
})();
