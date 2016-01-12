//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------


var o = new Array();
var a = new Object();

// Generate getter that will return a constructed string
function propString(i)
{
    return function() { var ret = "a" + i; return ret; };
}

function init(o, a)
{
    for (var i = 0; i < 21; i++)
    {
        // Create a replacer array that doesn't hold the string reference by using a getter to create
        // the string.
        Object.defineProperty(o, i, { get: propString(i) } );


        // Initialize the object to be stringify
        a["a" + i] = i;
    }
}

init(o,a);

WScript.Echo(JSON.stringify(a,o));

// Bug 30349 - invalid replacer array element after valid element causes crash regardless of input
WScript.Echo(JSON.stringify(true, [new Number(1.5), true])); // Original repro
WScript.Echo(JSON.stringify(false, [new Number(1.5), true]));
WScript.Echo(JSON.stringify(null, [new Number(1.5), true]));
// Valid input should just ignore any bad replacer array elements
WScript.Echo(JSON.stringify(a, [false, "a0", true, "a10", false]));
