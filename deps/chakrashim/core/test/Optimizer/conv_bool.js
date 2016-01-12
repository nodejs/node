//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------

function t1(c)
{
    if ((c != -1) != 1)
    {
    return true;
    }
    else
    {
    return false;
    }
}

function t2(c)
{
    if ((c <= -1) != 1)
    {
    return true;
    }
    else
    {
    return false;
    }
}

function t3(c)
{
    if ((c >= -1) != 1)
    {
    return true;
    }
    else
    {
    return false;
    }
}

if (!t1(1) && !t1(1) && t1(-1) &&
    t2(1) && t2(1) && !t2(-1) &&
    !t3(1) && !t3(1) && !t3(-1)
   )
{
    WScript.Echo("Passed");
}
else
{
    WScript.Echo("FAILED");
}
