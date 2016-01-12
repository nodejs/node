//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------

function AsmModule(stdlib) {
    "use asm";
    var m1 = stdlib.Math.fround;
    //Not operator testing
    function f1(){
        var x = m1(1.5);
        var y = 1;
        y = ~~~x
        return y|0
    }
    return f1;
}
var global = {Math:Math};
var asmModule = AsmModule(global);

WScript.Echo(asmModule());
WScript.Echo(asmModule());
