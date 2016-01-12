//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------

function test4()
{
    //Polymorphic Object Reference. 
    //Obj.C is a property on the prototype and will only get optimized.
   return obj.C + obj.F;
}

var obj = {D:5,F:2};
Object.prototype.C = 10;
WScript.Echo(test4());
obj.B = 5;
WScript.Echo(test4());


//JIT - Polymorphic Fixed Field for LdRootFld
WScript.Echo(test4());
obj.C = 99;
WScript.Echo(test4());


