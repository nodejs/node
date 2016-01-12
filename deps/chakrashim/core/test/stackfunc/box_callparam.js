//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------



function blah()
{

    function nested2()
    {
        return nested2;
    }
    function nested(a)
    {
        if (a) {
            throw 1;
        }
    }
    nested(nested2());
}


try
{
    blah();
}
catch (e)
{
}
