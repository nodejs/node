//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------

/*
*******************************UNIT TEST FOR SWITCH CASE OPTIMIZATION*******************************
*   Test for 3 switch cases bailing out during different times of the execution.
*/
function f(x,y,z)
{
    switch(x)
    {
        case 1:
           WScript.Echo(1);
           break;
        case 2:
           WScript.Echo(2);
           break;
        case 3:
           WScript.Echo(3);
           break;
        case 4:
           WScript.Echo(4);
           break;
        default:
            WScript.Echo('default-x');
            break;
    }

    switch(y)
    {
        case 1:
            WScript.Echo(1);
            break;
        case 2:
           WScript.Echo(2);
           break;
        case 3:
           WScript.Echo(3);
           break;
        case 4:
           WScript.Echo(4);
           break;
        default:
            WScript.Echo('default-y');
            break;
    }

    switch(z)
    {
        case 1:
            WScript.Echo(1);
            break;
        default:
            WScript.Echo('default-z');
            break;
    }
}

//making the first switch to get profiled as object during first run in the interpreter
f(1,2,new Object);
f(1,2,3);
f(1,2,3);
f(1,2,3);
f(1,2,3);

//making the second and third to bail out.
for(i=0;i<30;i++)
{
    f(1,new Object,3);
    f(new Object,new Object,3);
}
