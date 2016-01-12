//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------

function main()
{
    var a;        // a = undefined
    var e = 0;

    // We shouldn't invert this branch as relational comparison involving
    // undefined always returns false.
    if (!(a >= 1))
        e = true;

    WScript.Echo("e = " + e);
}

main();
