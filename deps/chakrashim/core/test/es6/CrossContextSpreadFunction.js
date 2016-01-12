//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------

function foo() 
{
    for(var i = 0; i < arguments.length;i++)
    {
        if(arguments[i] != i+1)
        {
            print("FAIL");
        }
    }
    print("PASS");
}
