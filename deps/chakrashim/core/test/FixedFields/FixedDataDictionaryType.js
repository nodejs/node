//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------

/*Dictionary Type Handler*/
var obj = {A:1, B:2} // PTH
function test2()
{
    return obj.A;
}
   
Object.defineProperty(obj, "D", {get: function() {return 5;}});//DTH

WScript.Echo(test2());
WScript.Echo(test2());
obj.A  =99;
WScript.Echo(test2());

