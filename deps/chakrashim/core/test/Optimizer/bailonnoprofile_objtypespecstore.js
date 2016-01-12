//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------


function test(o)
{
    var localCond =cond;
    o.p1 = 1;
    o.p2 = 2;
    o.p3 = 3;
    o.p4 = 4;
    o.p5 = 5;
    o.p6 = 6;
    o.p7 = 7;
    o.p8 = 8;
    if (localCond)
    {
        o.p9 = 9;
        nonExistGlobal();
    }
    else
    {
        o.p9 = 9;
    }
}

var cond = false;

test({});
cond = true;
try
{
test({});
}
catch(e)
{
}

cond = false;
test({});
