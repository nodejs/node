//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------

// Compile with -mic:1 -forcedeferparse to generate jitted code with stack funcs and stack closures.
// Then box after jitting and force jitted code to detect that it must allocate closures on heap.
function outer() {
    WScript.Echo('outer');
    function inner() {
        return inner;
    }
    if (i) return inner();
    i++;
}
var i = 0;
outer();
outer();
var f = outer();
WScript.Echo(typeof f());

