//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------


var x = { z: 0 };
function test(param)
{
    with (x)
    {
        z = function blah()
        {
            return param;
        }
    }

}


test("test1");
WScript.Echo(x.z());
test("test2");
WScript.Echo(x.z());
