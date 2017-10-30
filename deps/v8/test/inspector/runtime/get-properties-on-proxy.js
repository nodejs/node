// Copyright 2016 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

let {session, contextGroup, Protocol} = InspectorTest.start("Check that while Runtime.getProperties call on proxy object no user defined trap will be executed.");

contextGroup.addScript(`
var self = this;
function testFunction()
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
    return new Proxy({ a : 1}, handler);
}`);

Protocol.Runtime.evaluate({ expression: "testFunction()"}).then(requestProperties);

function requestProperties(result)
{
  Protocol.Runtime.getProperties({ objectId: result.result.objectId, generatePreview: true }).then(checkCounter);
}

function checkCounter(result)
{
  Protocol.Runtime.evaluate({ expression: "self.counter" }).then(dumpCounter);
}

function dumpCounter(result)
{
  InspectorTest.logMessage(result);
  InspectorTest.completeTest();
}
