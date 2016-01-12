//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------

var x = "d";
function foo()
{
    var s = unescape(x);
    if(s!='')
    {
        switch(s){
        case '0':
            WScript.Echo(0);
            break;
        case '1':
            WScript.Echo(1);
            break;
        case '2':
            WScript.Echo(2);
            break;
        case '3':
            WScript.Echo(3);
            break;
        default:
            break;
        }
    }
}
foo();
foo();
foo();
WScript.Echo("passed");
