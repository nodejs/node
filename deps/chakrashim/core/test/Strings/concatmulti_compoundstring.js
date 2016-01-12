//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------

function test()
{

    var cs = "a";
    for (var i = 0; i < 10; i++)
    {
        cs += i;        // create compound string
    }

    var a = "blahblahblah" + cs + "blahblahblah"; // concat node
    cs += "Z";  // modify compound string
    WScript.Echo(a); // make sure the concat string is still valid
}

test();
test();
