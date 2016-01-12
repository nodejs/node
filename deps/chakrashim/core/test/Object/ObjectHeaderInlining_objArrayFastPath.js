//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------

function AccessObjArray(obj)
{
    WScript.Echo(obj[0]);
}

function TwoProp(a,b)
{
    this.a = a;
    this.b = b;
}

//Passing the second param as JavascriptArray
var obj1 = new TwoProp({}, ["1",2,3]);

var obj2 = new Object();
obj2[0] = 10;

AccessObjArray(obj2);
AccessObjArray(obj2);
AccessObjArray(obj2);

AccessObjArray(obj1);