//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------

function foo()
{
    try
    {
        pnybsz_0 = new Uint8ClampedArray();
        pnybsz_0[-29] = Object(Symbol());
        pnybsz_0;
        pnybsz_0;
    }
    catch(e)
    {
        WScript.Echo(e.message);
    }
}
foo();
foo();
