//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------



function f()
{
}

f();

try
{
    f();
    throw 1;
}
catch (e)
{
    f();
}
finally
{
    f();
}

f();


