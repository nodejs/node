//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------

/*
 * Behaviour of inline caches when the TypeHandler switches to BigDictionaryTypeHandler. 
 * Based on a bug: Blue 179018
 */
var obj1 = { prop1: 0 };
var index = 0;//{};
Object.defineProperty(obj1, "someProp", { get: function () { }, set: function (x) { } });

var func0 = function () {
    obj1[index--] = 1;
    return obj1.prop1;
}
var func1 = function (obj1) {
    for (var i = 0; i < 65535; i++) {
        obj1.prop1 = func0();
    }
}
WScript.Echo("Pass");