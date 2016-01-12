//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------

WScript.LoadScriptFile("..\\UnitTestFramework\\UnitTestFramework.js");

function flattenArray(array) {
    return Array.prototype.concat.apply([], array);
}

var propertyNames = [
    "global",
    "ignoreCase",
    "multiline",
    "options",
    "source",
    "sticky",
    "unicode",
];

var tests = flattenArray(propertyNames.map(function (name) {
    // Values returned by the properties are tested in other files since they
    // are independent of the config flag and work regardless of where the
    // properties are.
    return [
        {
            name: name + " exists on the prototype",
            body: function () {
                var descriptor = Object.getOwnPropertyDescriptor(RegExp.prototype, name);
                assert.isNotUndefined(descriptor, "descriptor");

                assert.isFalse(descriptor.enumerable, name + " is not enumerable");
                assert.isTrue(descriptor.configurable, name + " is configurable");
                assert.isUndefined(descriptor.value, name + " does not have a value");
                assert.isUndefined(descriptor.set, name + " does not have a setter");

                var getter = descriptor.get;
                assert.isNotUndefined(getter, name + " has a getter");
                assert.areEqual('function', typeof getter, "Getter for " + name + " is a function");
                assert.areEqual("get " + name, descriptor.get.name, "Getter for " + name + " has the correct name");
                assert.areEqual(0, descriptor.get.length, "Getter for " + name + " has the correct length");
            }
        },
        {
            name: name + " does not exist on the instance",
            body: function () {
                var descriptor = Object.getOwnPropertyDescriptor(/./, name);
                assert.isUndefined(descriptor, name);
            }
        },
        {
            name: name + " getter can be called by RegExp subclasses",
            body: function () {
                class Subclass extends RegExp {}
                var re = new Subclass();
                assert.doesNotThrow(function () { re[name] });
            }
        },
        {
            name: name + " getter can not be called by non-RegExp objects",
            body: function () {
                var o = Object.create(/./);
                var getter = Object.getOwnPropertyDescriptor(RegExp.prototype, name).get;
                assert.throws(getter.bind(o));
            }
        },
        {
            name: name + " should be deletable",
            body: function () {
                var descriptor = Object.getOwnPropertyDescriptor(RegExp.prototype, name);
                delete RegExp.prototype[name];
                assert.isFalse(name in RegExp.prototype);
                Object.defineProperty(RegExp.prototype, name, descriptor);
            }
        }
    ];
}));

testRunner.runTests(tests, { verbose: WScript.Arguments[0] != 'summary' });
