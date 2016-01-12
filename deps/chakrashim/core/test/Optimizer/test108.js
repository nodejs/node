//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------

var runningJITtedCode = false;
function test0() {
    function makeArrayLength(x) { if(x < 1 || x > 4294967295 || x != x || isNaN(x) || !isFinite(x)) return 100; else return Math.floor(x) & 0xffff; };;
    var obj0 = {};
    var obj1 = {};
    var arrObj0 = {};
    var func2 = function(argFunc180) {
        // Runs JIT only code
        if(runningJITtedCode) {
            obj0.length = (-obj1.length);
        }

        (++obj0.length);
    }
    var floatary = [-1.5, -0.5, -0, 1.5, 12.987, 12.123, 100.33, 8.8, 5.5, 44.66, 42.24, 124.07, -0.99, 56.65, 42.24];
    var intfloatary = [1, 4, -1, -6, -0, +0, 55, -100, 2.56, -3.14, 6.6, 42, 2.3, 67, 1.97, -24, 77.99];
    var intary = [4, 66, 767, -100, 0, 1213, 34, 42, 55, -123, 567, 77, -234, 88, 11, -66];
    var a = 1;
    var b = 1;
    var c = 1;
    var d = 1;
    var e = 1;
    obj1.length = 2147483647;
    arrObj0.prop0 = 1;
    arrObj0.length = makeArrayLength(1);
    // Iterate through an array of arrays.
    function _array2iterate(_array2tmp) {
        for(var _array2i in _array2tmp) {
            if(_array2i.indexOf("method") == 0)
                continue;

            if(_array2tmp[_array2i] instanceof Array) {
                _array2iterate(_array2tmp[_array2i]);
                obj1 = arrObj0;
                // Snippets: arrayops.ecs
                intary = intary.concat(obj1.length, arrObj0.prop0, e, b, obj0.length, arrObj0.prop0, obj1.prop0);
                intary[20];
                var v502047 = intary.every(function(v502048) { return v502048 + arrObj0.length < obj1.prop0; }, this);
                ary = intfloatary.filter(function(v502050) { return 1; });
                v502047 = intary.forEach(function(v502051, v502052) { intary[v502052]++; });
                v502047 = ary.indexOf(b);
                v502047 = intfloatary.lastIndexOf(obj0.length, 5);
                v502047 = ary.join();
                v502047 = intary.map(function(v502049) { return d * v502049; });
                v502047 = intfloatary.reduce(function(v502053) { obj1.length = v502053 + obj0.length; return obj1.length; }, obj0.prop0);
                v502047 = intfloatary.reverse();
                v502047 = ary.unshift(obj0.length, e, e, a, c, d, e);
                v502047 = ary.shift();
                v502047 = floatary.slice(4, -3);
                v502047 = ary.some(function(v502054) { return v502054 < a; });
                v502047 = intfloatary.sort();
                v502047 = Array.isArray(floatary);
                v502047 = floatary.splice(obj0.length, d, e, d, e, e, a);
            }
            else {
                b += _array2tmp[_array2i];
            }
        }
    }
    _array2iterate([[1], [1], [1, 1, [func2.call(obj1, 1), func2.call(obj1, 1), 1, [arrObj0.length, 1, [arrObj0.length]]]]]);
};

test0();
runningJITtedCode = true;
test0();

WScript.Echo("pass");
