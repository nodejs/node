//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------

//Array sort testing for Object type
var x = new Object();
x = [8,41, 25, 7];
x.foo = Array.prototype.sort;
WScript.Echo(x.foo(function(a,b){return a - b}));

y = [9, 8, 4, 10, 190, 12, 20];
y.foo = Array.prototype.sort;
WScript.Echo(y.foo());

// Test sort on generic object
var objs = {
    "empty array": function () {
        return [];
    },

    "array with one undefined": function () {
        return [undefined];
    },

    "array with one null": function () {
        var o = [0];
        delete o[0];
        return o;
    },

    "array with two undefined": function () {
        return [undefined, undefined];
    },

    "array with multiple undefined": function () {
        var o = [undefined,,undefined,undefined,,,,undefined];
        return o;
    },

    "array with multiple null": function () {
        var o = [7,,5,2,,,6];
        for (var i = 0; i < o.length; i++) {
            delete o[i];
        }
        return o;
    },

    "array with mixed undefined and null": function () {
        var o = [undefined,1,,9,,3,8,undefined];
        delete o[0];
        return o;
    },

    "empty object": function () {
        return {
            length: 0
        };
    },

    "object with one undefined": function () {
        return {
            0: undefined,
            length: 1
        };
    },

    "object with one missing": function () {
        return {
            length: 1
        };
    },

    "object with undefined, missing": function () {
        return {
            0: undefined,
            length: 2
        };
    },

    "object with multiple undefined": function () {
        return {
            0: undefined,
            3: undefined,
            7: undefined,
            8: undefined,
            length: 10
        };
    },

    "adhoc object": function () {
        return {
            0: 7,
            2: 5,
            3: 2,
            6: 6,
            length: 10
        };
    },
};

function getObj(name) {
    var obj = objs[name]();
    obj.sort = Array.prototype.sort;
    obj.join = Array.prototype.join;
    obj.toString = Array.prototype.toString;
    return obj;
}

var echo = WScript.Echo;
echo();
for (var name in objs) {
    echo("Test " + name);
    echo(getObj(name).sort());
    echo(getObj(name).sort(function(a,b){return b - a;}));
    echo();
}
