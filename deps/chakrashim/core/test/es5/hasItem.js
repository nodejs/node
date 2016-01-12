//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------

var echo = WScript.Echo;

function testHasItem(o, prop) {
    try {
        Object.defineProperty(o, prop, {
            get: function () { echo(" ", "FAIL: THIS SHOULD NOT BE CALLED"); },
            configurable: false,
            enumerable: false
        });
    } catch (e) {
        echo(" ", "pass", "...can not defineProperty...", "(" + prop + ")");
        return;
    }
    echo(" ", o.hasOwnProperty(prop) && !o.propertyIsEnumerable(prop) ? "pass" : "fail",
        "(" + prop + ")");
}

var objs = {
    "empty object": function () {
        return {};
    },

    "empty array": function () {
        return [];
    },

    "number object": function () {
        return new Number(123);
    },

    "string object": function () {
        return new String("test");
    },

    "Object": function () {
        return Object;
    },

    "global object": function () {
        return this;
    },

    "object with 1 property": function () {
        return {
            hello: "world"
        };
    },

    "object with many properties": function () {
        var o = {};
        for (var i = 0; i < 50; i++) {
            Object.defineProperty(o, "prop" + i, {
                value: "value" + i
            });
        }
        return o;
    },

    "object with accessor": function () {
        return {
            get hello() { return "world"; },
            0: "value0",
            1: "value1",
            2: "value2"
        };
    },

    "array": function () {
        return [0, 1, 2, 3];
    },

    "es5 array": function () {
        var o = [0, 1, 2, 3];
        Object.defineProperty(o, 1, {
            get: function() { return "getter1"; }
        });
        return o;
    },
};

var props = [
    "abc",
    -1,
    0,
    1,
    10,
    0xfffffffe,
    "x y",
    "x\u0000y",
    "x\u0000\u0000y"
];

for (var obj in objs) {
    echo("Test " + obj);
    for(var prop in props) {
        testHasItem(objs[obj](), props[prop]);
    }
}
