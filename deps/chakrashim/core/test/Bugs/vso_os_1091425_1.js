//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------

(function addAccessorPropertiesToGlobal() {
    var getter = function () { throw new Error("This getter should not get called"); };
    var setter = function () { throw new Error("This setter should not get called"); };

    Object.defineProperty(this, "foo", {
        get: getter,
        set: setter,
        configurable: true
    });

    Object.defineProperty(this, "bar", {
        get: getter,
        set: setter,
        configurable: true
    });

    Object.defineProperty(this, "nonConfigurableFoo", {
        get: getter,
        set: setter,
        configurable: false
    });

    Object.defineProperty(this, "nonConfigurableBar", {
        get: getter,
        set: setter,
        configurable: false
    });

    // double check that the property is added as expected according to spec
    var d = Object.getOwnPropertyDescriptor(this, "foo");

    assertPropertyExists(d, 'get', getter);
    assertPropertyExists(d, 'set', setter);
    assertPropertyExists(d, 'configurable', true);
    assertPropertyExists(d, 'enumerable', false);
    assertPropertyDoesNotExist(d, 'writable');
    assertPropertyDoesNotExist(d, 'value');

    d = Object.getOwnPropertyDescriptor(this, "bar");

    assertPropertyExists(d, 'get', getter);
    assertPropertyExists(d, 'set', setter);
    assertPropertyExists(d, 'configurable', true);
    assertPropertyExists(d, 'enumerable', false);
    assertPropertyDoesNotExist(d, 'writable');
    assertPropertyDoesNotExist(d, 'value');

    var d = Object.getOwnPropertyDescriptor(this, "nonConfigurableFoo");

    assertPropertyExists(d, 'get', getter);
    assertPropertyExists(d, 'set', setter);
    assertPropertyExists(d, 'configurable', false);
    assertPropertyExists(d, 'enumerable', false);
    assertPropertyDoesNotExist(d, 'writable');
    assertPropertyDoesNotExist(d, 'value');

    var d = Object.getOwnPropertyDescriptor(this, "nonConfigurableBar");

    assertPropertyExists(d, 'get', getter);
    assertPropertyExists(d, 'set', setter);
    assertPropertyExists(d, 'configurable', false);
    assertPropertyExists(d, 'enumerable', false);
    assertPropertyDoesNotExist(d, 'writable');
    assertPropertyDoesNotExist(d, 'value');
}).call(this);
