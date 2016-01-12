//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------

WScript.LoadScriptFile("..\\UnitTestFramework\\UnitTestFramework.js");

var tests = [
   {
       name: "Reflect.Set",
       body: function () {
           var receiver = {};
           var fn = function () { };
           Object.defineProperty(receiver, 'p', { set: fn });

           assert.isFalse(Reflect.set({}, 'p', 42, receiver), "If the receiver object already has an accessor property then [[Set]] will return false");
           assert.isFalse(Reflect.set({ p: 43 }, 'p', 42, receiver), "For existing accessor property on receiver [[Set]] returns false even when an own descriptor exists on the target");
       }
   },
   {
       name: "Reflect.construct",
       body: function () {
           var o = {};
           var internPrototype;
           function fn() {
               assert.areEqual(new.target, Array, "The new.target for the invocation should be passed in as Array");
               this.o = o;
               internPrototype = Object.getPrototypeOf(this);
           }

           var result = Reflect.construct(fn, [], Array);
           assert.areEqual(Object.getPrototypeOf(result), Array.prototype, "Internal proto of the result object should be set according the newTarget");
           assert.areEqual(internPrototype, Array.prototype, "This object of the result is created from newTarget parameter and the internal proto is set from it");
           assert.areEqual(result.o, o, "Property set to this object during object construction should be retained");

           class A1 {
               constructor() {
                   assert.areEqual(new.target, Array, "The new.target for the base class should be passed in as Array");
                   assert.isTrue(this instanceof Array, "The internal proto of the base class's this should be set to the newTarget's prototype");
               }
           }
           var result = Reflect.construct(A1, [], Array);

           class A2 {}
           class B2 extends A2 {
               constructor() {
                   assert.areEqual(new.target, Array, "The new.target for the derived class should be passed in as Array");
                   super(); /* Initialize this */
                   assert.isTrue(this instanceof Array, "The internal proto of the derived class's this should be set to the newTarget's prototype");
               }
           }
           var result = Reflect.construct(B2, [], Array);

           class A3 {}
           class B3 extends A3 {
               constructor() {
                   assert.areEqual(new.target, B3, "The new.target for the derived class should be the same as the newTarget passed in to the construct call");
                   super();
                   assert.isTrue(this instanceof A3, "This should be properly initialized with the right base class");
                   assert.isTrue(this instanceof B3, "This should be properly instantiated using the derived class object");
               }
           }
           var result = Reflect.construct(B3, [], B3);
       }
   }
];

testRunner.runTests(tests, { verbose: WScript.Arguments[0] != "summary" });
