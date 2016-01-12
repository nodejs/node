//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------


var i = 0;
function test(param)
{
    var nested;
    function init(param2)
    {
        nested = function() { return param + param2; }
    }

    init(i++);
    return nested();
}


WScript.Echo(test("test1"));
WScript.Echo(test("test2"));
