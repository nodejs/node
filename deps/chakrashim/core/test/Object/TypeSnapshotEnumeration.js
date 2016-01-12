//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------

function test0(testName, objectTypeName, o) {
    // Basic enumeration with no changes during enumeration
    enumerate(testName, objectTypeName, o, false /* deleteQ */, false /* addQ */, false /* addX */);
}

function test1(testName, objectTypeName, o) {
    // Delete a property during enumeration
    enumerate(testName, objectTypeName, o, true /* deleteQ */, false /* addQ */, false /* addX */);
}

function test2(testName, objectTypeName, o) {
    // Delete and re-add a property during enumeration
    enumerate(testName, objectTypeName, o, true /* deleteQ */, true /* addQ */, false /* addX */);
}

function test3(testName, objectTypeName, o) {
    // Add a previously-deleted property during enumeration
    delete o.q;
    enumerate(testName, objectTypeName, o, false /* deleteQ */, true /* addQ */, false /* addX */);
}

function test4(testName, objectTypeName, o) {
    // Add a new property during enumeration
    enumerate(testName, objectTypeName, o, false /* deleteQ */, false /* addQ */, true /* addX */);
}

function test5(testName, objectTypeName, o) {
    // Delete a property and add a new property during enumeration
    enumerate(testName, objectTypeName, o, true /* deleteQ */, false /* addQ */, true /* addX */);
}

function test6(testName, objectTypeName, o) {
    // Delete a property before enumeration and add a new property during enumeration
    delete o.q;
    enumerate(testName, objectTypeName, o, false /* deleteQ */, false /* addQ */, true /* addX */);
}

function test7(testName, objectTypeName, o) {
    // Delete, re-add, and add a new property during enumeration
    enumerate(testName, objectTypeName, o, true /* deleteQ */, true /* addQ */, true /* addX */);
}

function test8(testName, objectTypeName, o) {
    // Add a previously-deleted property and a new property during enumeration
    delete o.q;
    enumerate(testName, objectTypeName, o, false /* deleteQ */, true /* addQ */, true /* addX */);
}

var supportsEs5 = !!Object.getOwnPropertyNames;

var ObjectType = {
    PathTypeObject: 0,
    SimpleDictionaryTypeObject: 1,
    DictionaryTypeObject: 2,
    Array: 3,
    Es5Array: 4,
    Arguments: 5,
    DeferredTypeObject: 6
};

for(var i = 0; ; ++i) {
    var testName = "test" + i;
    var runTest = this[testName];
    if(!runTest)
        break;

    for(var objectTypeName in ObjectType)
        runTest(testName, objectTypeName, createObject(ObjectType[objectTypeName]));
    echo();

    for(var objectTypeName in ObjectType)
        runTest(testName + "<unordered>", objectTypeName, toUnordered(createObject(ObjectType[objectTypeName])));
    echo();
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

function createObject(objectType) {
    var o;
    if(objectType === ObjectType.PathTypeObject)
        o = { p: 0, q: 0 };
    else if(objectType === ObjectType.SimpleDictionaryTypeObject) {
        o = { p: 0, q: 0, r: 0 };
        delete o.r;
    } else if(objectType === ObjectType.DictionaryTypeObject) {
        o = { p: 0 };
        if(supportsEs5)
            Object.defineProperty(
                o,
                "q",
                {
                    configurable: true,
                    enumerable: true,
                    get: function () { return 0; },
                    set: function (v) { }
                });
    } else if(objectType === ObjectType.Array || objectType === ObjectType.Es5Array) {
        o = [0, 0];
        o.p = 0;
        o.q = 0;
        if(objectType === ObjectType.Es5Array && supportsEs5)
            Object.defineProperty(
                o,
                "1",
                {
                    configurable: true,
                    enumerable: true,
                    writable: false
                });
    } else if(objectType === ObjectType.Arguments) {
        o = arguments;
        o.p = 0;
        o.q = 0;
    } else if(objectType === ObjectType.DeferredTypeObject)
        o = JSON;
    return o;
}

function toUnordered(o) {
    var p;
    for(var i = 0; i < 32; ++i) {
        p = "toUnordered" + i;
        o[p] = 0;
        delete o[p];
    }
    return o;
}

function enumerate(testName, objectTypeName, o, deleteQ, addQ, addX) {
    var testHeader = testName + "(" + objectTypeName + "): ";

    var a;
    if(supportsEs5) {
        a = [];
        for(var p in o)
            a.push(p);

        var ownPropertyNames = Object.getOwnPropertyNames(o);
        var ownPropertyNamesMinusUnenumerable = [];
        for(var i = 0; i < ownPropertyNames.length; ++i)
            if(Object.getOwnPropertyDescriptor(o, ownPropertyNames[i]).enumerable)
                ownPropertyNamesMinusUnenumerable.push(ownPropertyNames[i]);
        ownPropertyNames = ownPropertyNamesMinusUnenumerable;
        if(a.length === ownPropertyNames.length) {
            for(var i = 0; i < a.length; ++i) {
                if(a[i] !== ownPropertyNames[i]) {
                    echo(testHeader + "FAIL: enumeration yielded different set or order of properties compared with getOwnPropertyNames. GetOwnPropertyNames returned:");
                    echo(testHeader + arrayToString(ownPropertyNames));
                    break;
                }
            }
        }
        else {
            echo(testHeader + "FAIL: enumeration yielded different number of properties compared with getOwnPropertyNames. GetOwnPropertyNames returned:");
            echo(testHeader + arrayToString(ownPropertyNames));
        }
    }

    a = [];
    for(var p in o) {
        a.push(p);
        if(deleteQ) {
            deleteQ = false;
            delete o.q;
        }
        if(addX) {
            addX = false;
            o.x = 0;
        }
        if(addQ) {
            addQ = false;
            o.q = 0;
        }
    }

    a.sort(); // don't verify the absolute enumeration order since it doesn't matter according to the spec
    var a2 = [];
    for(var i = 0; i < a.length; ++i)
        a2.push(a[i]);
    echo(testHeader + arrayToString(a2));
}

function arrayToString(a) {
    var s = "[";
    for(var i = 0; i < a.length; ++i) {
        if(i)
            s += ", ";
        s += a[i];
    }
    return s + "]";
}

function echo() {
    var doEcho;
    if(this.WScript)
        doEcho = function (s) { this.WScript.Echo(s); };
    else if(this.document)
        doEcho = function (s) {
            var div = this.document.createElement("div");
            div.innerText = s;
            this.document.body.appendChild(div);
        };
    else
        doEcho = function (s) { this.print(s); };
    echo = function () {
        var s = "";
        for(var i = 0; i < arguments.length; ++i)
            s += arguments[i];
        doEcho(s);
    };
    echo.apply(this, arguments);
}
