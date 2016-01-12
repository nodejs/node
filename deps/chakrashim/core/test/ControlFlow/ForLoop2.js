//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------

for (;;)
{
        for(var a = 10; a >= 0; --a)
        {
                for(var b = 0; b < 10; b += 4)
                {
                        for(var c = 0; c < 10; c += 3)
                        {
                                WScript.Echo(a,b,c);
                        }
                        b--;
                        b--;
                }
        }
        break;
}
