//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------

var failed = false;
var verifyMemoryUsage = false;
var maxHashTableSizeShift = 5;
var maxHashTableSize = 1 << maxHashTableSizeShift;

function createHashTable() {
    var o = new Array(); // array so that we can transition its type handler to an ES5 array type handler
    var elements = new Array(maxHashTableSize);
    o.elementHash = function (i) {
        return Math.abs(i) & (maxHashTableSize - 1);
    };
    o.add = function (i) {
        var elementIndex = this.elementHash(i);
        if(elements[elementIndex] !== undefined) {
            // Delete up to 4 used elements to make room
            for(var j = elementIndex; j < elementIndex + 4 && j < maxHashTableSize; ++j) {
                var e = elements[j];
                if(e === undefined)
                    continue;
                var h = "h" + e;
                assertAreEqual(e, this[h]);
                elements[j] = undefined;
                delete this[h];
            }
        }
        elements[elementIndex] = i;
        this["h" + i] = i;
    };
    o.verify = function () {
        for(var i = 0; i < maxHashTableSize; ++i) {
            var e = elements[i];
            if(e !== undefined)
                assertAreEqual(e, this["h" + e]);
        }
        for(var h in this) {
            if(h[0] !== "h")
                continue;
            assertAreEqual(h, "h" + elements[this.elementHash(this[h])]);
        }
    };
    return o;
}

function useAsHashTable(o, n) {
    for(var i = 0; n === 0 || i !== n; i = (i + 1) | 0) {
        for(var j = i; j !== (i + 4) | 0; j = (j + 1) | 0)
            o.add(j);
        if(!(i & 0xffff) && verifyMemoryUsage)
            WScript.Echo(i);
    }
    o.verify();
}

var o = createHashTable();
useAsHashTable(o, verifyMemoryUsage ? 0 : 1024);

// Transition to a DictionaryTypeHandler
Object.defineProperty(
    o,
    "foo",
    { configurable: true, enumerable: true, get: function () { }, set: function (v) { } });
useAsHashTable(o, 1024);

// Transition to an ES5ArrayTypeHandler
Object.defineProperty(
    o,
    "0",
    { configurable: true, writable: false, enumerable: true });
useAsHashTable(o, 1024);

if(!failed)
    WScript.Echo("pass");

function assertAreEqual(expected, actual) {
    if(expected === actual)
        return;
    failed = true;
    WScript.Echo("Expected: " + expected + ", Actual: " + actual);
}
