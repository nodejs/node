//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------

function AsmModule() {
    "use asm";
    function bd() {
        return +0;
    }    
    function foo()
    {
        var y = 5.5;
        y = +bd();
        return +y;
    }
    return foo;
}

var asmModule = AsmModule();
if(asmModule() == 0)
{
    WScript.Echo("pass");
}





