//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------

// test loop bounds that involve tagged integers.

// relational comparison
WScript.Echo("relational");
j = 0;
for(var i = 0x1ffffffc; i < 0x20000003; ++i)
{
    WScript.Echo(++j);
}
j = 0;
for(var i = -0x20000003; i < -0x1ffffffc; ++i)
{
    WScript.Echo(++j);
}

// equality comparison
WScript.Echo("equality");
j = 0;
for(var i = 0x1ffffffc; i != 0x20000003; ++i)
{
    WScript.Echo(++j);
}
j = 0;
for(var i = -0x20000003; i != -0x1ffffffc; ++i)
{
    WScript.Echo(++j);
}
