//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------

//Refer to Bug EZE\123
//This test case is to ensure that the length of the array is not always stored as a tagged integer
var id=0;
function verify(get_actual,get_expected,testid,testdesc)
{

    if(get_actual !==get_expected)
        WScript.Echo(testid+":"+testdesc+"\t"+"failed");
}

//Test Case 1: Testing Array of length 2^29 (max length of the tagged integer)

var arr1=new Array(536870912)
verify(arr1.length,536870912,id++, "\"Testing Array of length 2^29 \"");

// Test Case 2:Testing Array of length 2^29+1(One more than the max length of the tagged integer)

var arr2=new Array(536870913)
verify(arr2.length, 536870913, id++,"\"Testing Array of length 2^29+1 \"");

// Test Case 3:Tetsing an array of length 2^29 -1 (One less than the max length of the tagged integer)

var arr3=new Array(536870911)
verify(arr3.length, 536870911, id++,"\"Testing Array of length 2^29-1 \"");

//Test Case 4:Testing an array of lenght 2^32-1 ( max length of integer)
var arr4=new Array(4294967295)
verify(arr4.length ,4294967295, id++,"\"Testing Array of length 2^32-1 \"");

//Test Case 5:Testing an array of length greater than the max length of the Integer

try
{
    var arr5=new Array(1024*1024*1024*4)
    verify(1,0,"\"Testing an array of length >2^32 Did not raise an exception\"")
}
catch(e)
{
    verify(arr5,undefined,id++,"\"Testing an array of length greater than the max length of the Integer\"")
}

//Test Case 6:Testing Array of length 0 (Ensure tagged integers do not include the control bit as their length)

var arr6=new Array(0)
verify(arr6.length, 0, id++,"\"Testing Array of length 0 \"");

//Test Case 7:Testing Array of length 2^29 after changing the legth property

var arr7=new Array(536870911)
arr7.length=536870912
verify(arr7.length, 536870912,id++, "\"Testing Array of length 2^29 after changing the legth property \"")

//test case 8 testing array of length -1
try
{
var arr8=new Array(3);
arr8.length=-1
verify(1,0,"\"Testing an array length property with -1 Did not raise an exception\"")
}
catch(e)
{
verify(arr8.length, 3, id++,"\"Testing negative array legth property \"")

}

WScript.Echo("Done");

