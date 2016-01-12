//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------

var test_Neg_A = function(o, c)
{
    var y = 10;
    for (var i = 0; i < 2; i++)
    {
        y = o.x + (-c.prop);
    }
    return y;
}
test_Neg_A.test_result1 = true;

test_Neg_A();
