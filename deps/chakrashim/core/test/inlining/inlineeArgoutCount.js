//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------

function bar(){
    this.func.apply(this, arguments);
}

var a = new Object();
a.b = bar;
a.b.prototype.func = function(){};


function foo()
{
    new a.b(0,1,2,3,4,5,6,7,8,9);
}

foo();
foo();
foo();

WScript.Echo("Passed");