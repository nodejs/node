//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------

/*************Test1 - Creates Simple Dictionary TypeHandler for Prototype*****************/
function myObject()
{
    this.A = 1
} // PTH

myObject.prototype = {C:10};
Object.defineProperty(myObject.prototype, "B", {
    enumerable   : false,
    configurable : true,
    writable     : true, 
    value        : 20
}); //SDTH


var child = new myObject();//SDTH

function test1()
{
    return child.B;
}

WScript.Echo(test1());
WScript.Echo(test1());
child.B = 99;
WScript.Echo(test1());

/*************Test2 - Creates a Dictionary TypeHandler for Prototype*****************/
function myObject()
{
    this.A = 1,
    this.C = 10
    
 }; 
 
myObject.prototype = {B:10}
Object.defineProperty(myObject.prototype, "D", {get: function() {return 5;}});//DTH

function test2()
{
    return child.B;
}
   
var child = new myObject(); //DTH

WScript.Echo(test2());
WScript.Echo(test2());

child.B =99;
WScript.Echo(test2());

