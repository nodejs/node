//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------

// Verfies the order of getOwnPropertyDescriptor trap called for ownKeys
function test1() {
    var obj = {};
    var proxy = new Proxy(obj, {
        getOwnPropertyDescriptor: function (target, property) {
            print('getOwnPropertyDescriptor on proxy : ' + property.toString());
            return { configurable: true, enumerable: true, value: 10 };
        },

        ownKeys: function (target) {
            print('ownKeys for proxy');
            return ["prop0", "prop1", Symbol("prop2"), Symbol("prop5")];
        }
    });

    var proxy2 = new Proxy(proxy, {
        getOwnPropertyDescriptor: function (target, property) {
            print('getOwnPropertyDescriptor on proxy2 : ' + property.toString());
            return { configurable: true, enumerable: true, value: 10 };
        },

        ownKeys: function (target) {
            print('ownKeys for proxy2');
            return ["prop2", "prop3", Symbol("prop4"), Symbol("prop5")];
        }
    });

    print('***Testing Object.getOwnPropertyNames()');
    print(Object.getOwnPropertyNames(proxy2));
    print('***Testing Object.keys()');
    print(Object.keys(proxy2));
    print('***Testing Object.getOwnPropertySymbols()');
    print(Object.getOwnPropertySymbols(proxy2).length);
   
    print('***Testing Object.freeze()');
    try{
        Object.freeze(proxy2);
        print('Object.freeze should fail because underlying OwnPropertyKeys should fail since target becomes non-extensible');
    } catch (e) {
        if (!(e instanceof TypeError)) {
            $ERROR('incorrect instanceof Error' + e);
        }
    }
}

// Verifies that we don't call GetPropertyDescriptor of target for Object.keys unnecessarily if we throw TypeERROR before
function test2() {
    var obj = {};
    Object.defineProperty(obj, "a", { value: 5, configurable: false });
    var proxy = new Proxy(obj, {
        getOwnPropertyDescriptor: function (target, property) {
            print('getOwnPropertyDescriptor on proxy : ' + property.toString());
            return Object.getOwnPropertyDescriptor(target, property);
        },

        ownKeys: function (target) {
            print('ownKeys for proxy');
            return ["a", "prop0", "prop1", Symbol("prop2"), Symbol("prop5")];
        }
    });


    var proxy2 = new Proxy(proxy, {
        getOwnPropertyDescriptor: function (target, property) {
            print('getOwnPropertyDescriptor on proxy2 : ' + property.toString());
            return Object.getOwnPropertyDescriptor(target, property);
        },

        ownKeys: function (target) {
            print('ownKeys for proxy2');
            return ["prop2", "prop3", Symbol("prop4"), Symbol("prop5")];
        }
    });

    print('***Testing Object.keys()');
    try{
        print(Object.keys(proxy2));
        print('Should throw TypeError because ownKeys doesnt return non-configurable key.');
    } catch (e) {
        if (!(e instanceof TypeError)) {
            print('incorrect instanceof Error');
        }
    }
}

function test3() {
    var obj = {};
    var count = 0;
    var proxy = new Proxy(obj, {

        get: function (target, property, receiver) {
            print('get on proxy : ' + property.toString());
            return count++ * 5;
        },
        getOwnPropertyDescriptor: function (target, property) {
            print('getOwnPropertyDescriptor on proxy : ' + property.toString());
            return { configurable: true, enumerable: true, value: 10 };
        },

        ownKeys: function (target) {
            print('ownKeys for proxy');
            return ["prop0", "prop1", Symbol("prop2"), Symbol("prop5")];
        }
    });

    var proxy2 = new Proxy(proxy, {
        get: function (target, property, receiver) {
            print('get on proxy2 : ' + property.toString());
            return Reflect.get(target, property, receiver);
        },
        getOwnPropertyDescriptor: function (target, property) {
            print('getOwnPropertyDescriptor on proxy2 : ' + property.toString());

            return { configurable: true, enumerable: true, value: 10 };
        },

        ownKeys: function (target) {
            print('ownKeys for proxy2');
            return ["prop2", "prop3",  Symbol("prop4"), Symbol("prop5")];
        }
    });

    print('***Testing Object.assign()');
    var answer = Object.assign(obj, null, proxy, proxy2);
    var symbols = Object.getOwnPropertySymbols(answer);
    var names = Object.getOwnPropertyNames(answer);
    print("PropertyNames returned : ");
    for (i = 0; i < names.length; i++)
    {
        print(names[i].toString())
    }
    print("PropertySymbols returned : ");
    for (i = 0; i < symbols.length; i++)
    {
        print(symbols[i].toString())
    }
   
}

test1();
test2();
test3();