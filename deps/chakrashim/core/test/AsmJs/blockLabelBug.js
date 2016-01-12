//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------

function module()
{
    "use asm"
    function foo()
    {
      h:{
            switch (1) {
            case 1:
            {
                break h
            }

            }

        }
    }
    return foo;
}
var obj = module();
obj();
WScript.Echo("Pass");