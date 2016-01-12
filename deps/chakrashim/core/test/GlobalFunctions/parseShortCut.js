//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------

function Test(i)
{
    WScript.Echo(parseInt(i).toString());
    WScript.Echo(parseFloat(i).toString());
    WScript.Echo(parseInt(-i).toString());
    WScript.Echo(parseFloat(-i).toString());
}
Test(400012312542180394829e30);
Test(4.00012312542180394829e21);
Test(4.00012312542180394829e20);
Test(9.9999999999999999999e20);
Test(1e20);
Test(Infinity);
Test(NaN);

Test(0.00001);
Test(0.000001);
Test(0.0000001);
Test(0.0000000000000001);
